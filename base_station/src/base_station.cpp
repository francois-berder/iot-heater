#include "base_station.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <net/if.h>
#include <poll.h>
#include <sstream>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <vector>

#ifndef GIT_HASH
#define GIT_HASH        "development"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME      "unknown-time"
#endif

#ifndef BASE_STATION_PIN
#define BASE_STATION_PIN    "1234"
#endif

#define MESSAGE_SIZE    (64)

#define STATE_FILE_PATH     "/var/lib/base_station.state"

#define CHECK_WIFI_PERIOD       (60 * 1000)
#define NETWORK_INTERFACE_NAME  "wlan0"

struct __attribute__((packed)) message_header_t {
    uint8_t version;
    uint8_t type;
    uint8_t mac_addr[6];
    uint64_t counter;
};

namespace {

/* Check that phone numbers consist of 10 to 14 digits */
bool check_phone_number_format(const std::string &no)
{
    if (no.length() < 10 || no.length() > 14)
        return false;
    for (unsigned int i = 0; i < no.length(); ++i) {
        if (no[i] < '0' || no[i] > '9')
            return false;
    }

    return true;
}

/* From https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c */
bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool check_heater_name(const std::string &name)
{
    if (name.empty())
        return false;

    for (unsigned int i = 0; i < name.length(); ++i) {
        bool is_char_valid = (name[i] >= 'a' && name[i] <= 'z')
                          || (name[i] >= 'A' && name[i] <= 'Z')
                          || (name[i] >= '0' && name[i] <= '9');

        if (!is_char_valid)
            return false;
    }

    return true;
}
}

enum MessageType {
    REQ_HEATER_STATE    = 1,
    HEATER_STATE_REPLY  = 2,
};

BaseStation::BaseStation():
m_connections(),
m_connections_mutex(),
m_commands(),
m_commands_mutex(),
m_stale_timer(),
m_heater_default_state(HEATER_DEFROST),
m_heater_state(),
m_locked(false),
m_phone_whitelist(),
m_emergency_phone(),
m_check_wifi_timer(),
m_wifi_not_good_counter(0),
m_message_counter(0),
m_heater_counter()
{
    if (!loadState())
        saveState();

    /* Start check wifi timer */
    m_check_wifi_timer.start(CHECK_WIFI_PERIOD, true);

    /* Initialize message counter */
    srand(time(NULL));
    m_message_counter = rand();
    m_message_counter <<= 32;
}

BaseStation::~BaseStation()
{
    /* Close all file descriptors */
    for (auto& conn : m_connections)
        close(conn.fd);
}

void BaseStation::process()
{
    handleConnections();
    handleTimers();
    parseCommands();
    checkWifi();
}

/*
 * Beware this function is called from device_server context !
 * That is why we need a mutex around the use of m_devices.
 */
void BaseStation::handleNewDevice(int fd)
{
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    DeviceConnection conn;
    conn.fd = fd;
    conn.last_seen = std::chrono::steady_clock::now();

    m_connections.push_back(conn);
}

/*
 * Beware this function is called from sms_receiver context !
 */
void BaseStation::handleSMSCommand(const std::string &from, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_commands_mutex);
    m_commands.emplace(from, content);
}

void BaseStation::handleConnections()
{
    struct pollfd *fds;
    int nfds;

    {
        std::lock_guard<std::mutex> guard(m_connections_mutex);

        if (m_connections.empty())
            return;

        nfds = m_connections.size();
        fds = new struct pollfd[nfds];
        int i = 0;
        for (auto &conn : m_connections) {
            fds[i].fd = conn.fd;
            fds[i].events = POLLIN;
            ++i;
        }
    }

    int ret = poll(fds, nfds, 0);
    if (ret <= 0) {
        delete[] fds;
        return;
    }

    for (int i = 0; i < nfds; ++i) {
        if (!(fds[i].revents & POLLIN))
            continue;

        {
            std::lock_guard<std::mutex> guard(m_connections_mutex);

            for (auto itor = m_connections.begin(); itor != m_connections.end(); ++itor) {
                if (itor->fd != fds[i].fd)
                    continue;
                uint8_t data[MESSAGE_SIZE];
                int ret = read(fds[i].fd, data, sizeof(data));
                if (ret == 0) {
                    close(fds[i].fd);
                    m_connections.erase(itor);
                } else if (ret == sizeof(data)) {
                    parseMessage(*itor, data);
                }
                break;
            }
        }
    }

    delete[] fds;
}

void BaseStation::handleTimers()
{
    struct pollfd fds[1];

    fds[0].fd = m_stale_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret > 0) {
        for (unsigned i = 0; i < sizeof(fds)/sizeof(fds[0]); ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;

            uint64_t tmp;
            ret = read(fds[i].fd, &tmp, sizeof(tmp));
            if (ret < 0)
                continue;

            if (i == 0)
                checkStaleConnections();
        }
    }
}

void BaseStation::parseMessage(DeviceConnection &conn, uint8_t *data)
{
    struct message_header_t header;

    /* Parse header */
    header.version = *data++;
    header.type = *data++;
    memcpy(header.mac_addr, data, sizeof(header.mac_addr));
    data += sizeof(header.mac_addr);
    memcpy(&header.counter, data, sizeof(header.counter));
    data += sizeof(header.counter);

    if (header.version != 1) {
        std::stringstream msg;
        msg << "Ignoring message. Version " << header.version << " not supported.";
        Logger::warn(msg.str());
        return;
    }

    if (header.type == MessageType::REQ_HEATER_STATE) {
        conn.last_seen = std::chrono::steady_clock::now();

        /* Parse optional name */
        std::string name;
        {
            unsigned int i = 0;
            while (data[i] != 0xFF && data[i] != '\0')
                name += toupper(data[i++]);
        }

        sendHeaterState(conn.fd, name);

        /* Check if device rebooted since last message */
        uint64_t mac_addr;
        mac_addr = ((uint64_t)header.mac_addr[5] << 40LU)
                | ((uint64_t)header.mac_addr[4] << 32LU)
                | ((uint64_t)header.mac_addr[3] << 24LU)
                | ((uint64_t)header.mac_addr[2] << 16LU)
                | ((uint64_t)header.mac_addr[1] << 8LU)
                | ((uint64_t)header.mac_addr[0]);

        auto it = m_heater_counter.find(mac_addr);
        if (it == m_heater_counter.end()) {
            m_heater_counter[mac_addr] = header.counter;
        } else if (it->second - header.counter > 3) {
            std::stringstream ss;
            ss << "It seems that device "
            << std::hex << header.mac_addr[5] << ':'
            << std::hex << header.mac_addr[4] << ':'
            << std::hex << header.mac_addr[3] << ':'
            << std::hex << header.mac_addr[2] << ':'
            << std::hex << header.mac_addr[1] << ':'
            << std::hex << header.mac_addr[0] << ':'
            << " rebooted a few minutes ago.";
            Logger::warn(ss.str());
            
            std::stringstream msg;
            msg << "Warning!\n"
            << "Device " << name << ' '
            << std::hex << header.mac_addr[5] << ':'
            << std::hex << header.mac_addr[4] << ':'
            << std::hex << header.mac_addr[3] << ':'
            << std::hex << header.mac_addr[2] << ':'
            << std::hex << header.mac_addr[1] << ':'
            << std::hex << header.mac_addr[0] << ':'
            << " probably rebooted a few minutes ago.";
            SMSSender::instance().sendSMS(m_emergency_phone, msg.str());
            m_heater_counter[mac_addr] = header.counter;
        }
    } else if (header.type == MessageType::HEATER_STATE_REPLY) {
        std::stringstream ss;
        ss << "Ignoring HEATER_STATE_REPLY message from device "
        << std::hex << header.mac_addr[5] << ':'
        << std::hex << header.mac_addr[4] << ':'
        << std::hex << header.mac_addr[3] << ':'
        << std::hex << header.mac_addr[2] << ':'
        << std::hex << header.mac_addr[1] << ':'
        << std::hex << header.mac_addr[0];
        Logger::err(ss.str());
    } else {
        std::stringstream ss;
        ss << "Received unknown message type " << header.type << " from device "
        << std::hex << header.mac_addr[5] << ':'
        << std::hex << header.mac_addr[4] << ':'
        << std::hex << header.mac_addr[3] << ':'
        << std::hex << header.mac_addr[2] << ':'
        << std::hex << header.mac_addr[1] << ':'
        << std::hex << header.mac_addr[0];
        Logger::warn(ss.str());
    }
}

void BaseStation::parseCommands()
{
    while (!m_commands.empty()) {
        std::string from;
        std::string content;

        std::tie(from, content) = m_commands.front();
        m_commands.pop();

        /* Check phone belongs to whitelist */
        if (m_locked && !m_phone_whitelist.empty() && m_phone_whitelist.find(from) == m_phone_whitelist.end()) {
            std::stringstream ss;
            ss << "Received SMS from phone number \"" << from << "\" not in whitelist";
            Logger::warn(ss.str());
            return;
        }

        /* Trim content */
        content.erase(content.begin(), std::find_if(content.begin(), content.end(), [] (unsigned char c){ return !std::isspace(c); }));
        content.erase(std::find_if(content.rbegin(), content.rend(),
                      [] (unsigned char c){ return !std::isspace(c); }).base(), content.end());

        /* Convert all lowercase characters to uppercase */
        for (auto & c: content) c = toupper(c);

        if (content == "PING")
            SMSSender::instance().sendSMS(from, "PONG");
        else if (content == "VERSION")
            sendVersion(from);
        else if (content == "ALL OFF") {
            m_heater_default_state = HEATER_OFF;
            for (auto &e : m_heater_state)
                e.second = HEATER_OFF;
            saveState();
            SMSSender::instance().sendSMS(from, "ALL OFF");
        } else if (content == "ALL ECO") {
            m_heater_default_state = HEATER_ECO;
            for (auto &e : m_heater_state)
                e.second = HEATER_ECO;
            saveState();
            SMSSender::instance().sendSMS(from, "ALL ECO");
        } else if (content == "ALL DEFROST") {
            m_heater_default_state = HEATER_DEFROST;
            for (auto &e : m_heater_state)
                e.second = HEATER_DEFROST;
            saveState();
            SMSSender::instance().sendSMS(from, "ALL DEFROST");
        } else if (content == "ALL COMFORT") {
            m_heater_default_state = HEATER_COMFORT;
            for (auto &e : m_heater_state)
                e.second = HEATER_COMFORT;
            saveState();
            SMSSender::instance().sendSMS(from, "ALL COMFORT");
        } else if (content == "ALL ON") {
            m_heater_default_state = HEATER_COMFORT;
            for (auto &e : m_heater_state)
                e.second = HEATER_COMFORT;
            saveState();
            SMSSender::instance().sendSMS(from, "ALL ON");
        } else if (content.rfind("HEATER ", 0) == 0 && ends_with(content, " OFF")) {
            std::string name;
            name = content.substr(7);
            name = name.substr(0, name.length() - 4);

            if (check_heater_name(name)) {
                m_heater_state[name] = HEATER_OFF;
                saveState();
                std::stringstream reply;
                reply << "HEATER " << name << " OFF";
                SMSSender::instance().sendSMS(from, reply.str());
            } else {
                SMSSender::instance().sendSMS(from, "Invalid heater name");
            }
        } else if (content.rfind("HEATER ", 0) == 0 && ends_with(content, " ECO")) {
            std::string name;
            name = content.substr(7);
            name = name.substr(0, name.length() - 4);

            std::stringstream m;
            m << '\"' << name << '\"' << '\n';
            Logger::debug(m.str());

            if (check_heater_name(name)) {
                m_heater_state[name] = HEATER_ECO;
                saveState();
                std::stringstream reply;
                reply << "HEATER " << name << " ECO";
                SMSSender::instance().sendSMS(from, reply.str());
            } else {
                SMSSender::instance().sendSMS(from, "Invalid heater name");
            }
        } else if (content.rfind("HEATER ", 0) == 0 && ends_with(content, " DEFROST")) {
            std::string name;
            name = content.substr(7);
            name = name.substr(0, name.length() - 8);

            if (check_heater_name(name)) {
                m_heater_state[name] = HEATER_DEFROST;
                saveState();
                std::stringstream reply;
                reply << "HEATER " << name << " DEFROST";
                SMSSender::instance().sendSMS(from, reply.str());
            } else {
                SMSSender::instance().sendSMS(from, "Invalid heater name");
            }
        } else if (content.rfind("HEATER ", 0) == 0 && ends_with(content, " COMFORT")) {
            std::string name;
            name = content.substr(7);
            name = name.substr(0, name.length() - 8);

            if (check_heater_name(name)) {
                m_heater_state[name] = HEATER_COMFORT;
                saveState();
                std::stringstream reply;
                reply << "HEATER " << name << " COMFORT";
                SMSSender::instance().sendSMS(from, reply.str());
            } else  {
                SMSSender::instance().sendSMS(from, "Invalid heater name");
            }
        } else if (content.rfind("HEATER ", 0) == 0 && ends_with(content, " ON")) {
            std::string name;
            name = content.substr(7);
            name = name.substr(0, name.length() - 3);

            if (check_heater_name(name)) {
                m_heater_state[name] = HEATER_COMFORT;
                saveState();
                std::stringstream reply;
                reply << "HEATER " << name << " ON";
                SMSSender::instance().sendSMS(from, reply.str());
            } else {
                SMSSender::instance().sendSMS(from, "Invalid heater name");
            }
        } else if (content == "GET DEFAULT") {
            switch (m_heater_default_state) {
            case HEATER_OFF:
                SMSSender::instance().sendSMS(from, "DEFAULT: OFF");
                break;
            case HEATER_DEFROST:
                SMSSender::instance().sendSMS(from, "DEFAULT: DEFROST");
                break;
            case HEATER_ECO:
                SMSSender::instance().sendSMS(from, "DEFAULT: ECO");
                break;
            case HEATER_COMFORT:
                SMSSender::instance().sendSMS(from, "DEFAULT: COMFORT/ON");
                break;
            }
        } else if (content.rfind("GET HEATER ", 0) == 0) {
            std::string name;

            name = content.substr(11);

            if (check_heater_name(name)) {
                for (unsigned int i = 0; i < name.length(); ++i)
                    name[i] = toupper(name[i]);

                auto it = m_heater_state.find(name);
                HeaterState state;
                if (it != m_heater_state.end())
                    state = it->second;
                else
                    state = m_heater_default_state;

                std::stringstream msg;
                switch (state) {
                case HEATER_OFF: msg << "HEATER " << name << " OFF"; break;
                case HEATER_DEFROST: msg << "HEATER " << name << " DEFROST"; break;
                case HEATER_ECO: msg << "HEATER " << name << " ECO"; break;
                case HEATER_COMFORT: msg << "HEATER " << name << " COMFORT/ON"; break;
                }
                SMSSender::instance().sendSMS(from, msg.str());
            } else {
                SMSSender::instance().sendSMS(from, "Invalid name");
            }
        } else if (content == "GET IP") {
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("curl ifconfig.me", "r"), pclose);
            if (!pipe) {
                SMSSender::instance().sendSMS(from, "Fail to get public IP");
            } else {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
                    result += buffer.data();
                if (result.size() > 64)
                    result.resize(64);
                if (result.empty())
                    SMSSender::instance().sendSMS(from, "Unable to get public IP");
                else
                    SMSSender::instance().sendSMS(from, result);
            }
        } else if (content == "LOCK") {
            if (m_phone_whitelist.find(from) != m_phone_whitelist.end()) {
                SMSSender::instance().sendSMS(from, "LOCKED");
                m_locked = true;
            } else {
                SMSSender::instance().sendSMS(from, "Cannot lock: phone number is not whitelisted. Use ADD PHONE command.");
            }
        } else if (content.rfind("UNLOCK ", 0) == 0) {
            std::istringstream iss(content);
            std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                            std::istream_iterator<std::string>{}};
            Logger::debug(tokens[1]);
            if (tokens.size() >= 2) {
                if (tokens[1] == BASE_STATION_PIN) {
                    SMSSender::instance().sendSMS(from, "UNLOCKED");
                    m_locked = false;
                } else {
                    SMSSender::instance().sendSMS(from, "Wrong PIN");
                }
            }
        } else if (content.rfind("ADD PHONE ", 0) == 0) {
            if (!m_locked) {
                std::istringstream iss(content);
                std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                                std::istream_iterator<std::string>{}};
                if (tokens.size() == 3) {
                    std::string phone_number = tokens[2];
                    if (check_phone_number_format(phone_number)) {
                        m_phone_whitelist.insert(phone_number);
                        std::stringstream ss;
                        ss << "Phone number \"" << phone_number << "\" added to whitelist";
                        SMSSender::instance().sendSMS(from, ss.str());
                        saveState();
                    } else {
                        std::stringstream ss;
                        ss << "Phone number \"" << phone_number << "\" is not valid. Phone numbers must follow this format: (country code)(9-10 digits). Example: 3310203040506";
                        SMSSender::instance().sendSMS(from, ss.str());
                    }
                }
            }
        } else if (content.rfind("REMOVE PHONE ", 0) == 0) {
            if (!m_locked) {
                std::istringstream iss(content);
                std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                                std::istream_iterator<std::string>{}};
                if (tokens.size() == 3) {
                    m_phone_whitelist.erase(tokens[2]);
                    std::stringstream ss;
                    ss << "Phone number \"" << tokens[2] << "\" removed from whitelist";
                    SMSSender::instance().sendSMS(from, ss.str());
                    saveState();
                }
            }
        } else if (content.rfind("SET EMERGENCY PHONE ", 0) == 0) {
            std::istringstream iss(content);
            std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                            std::istream_iterator<std::string>{}};
            if (tokens.size() == 4) {
                std::string phone_number = tokens[3];
                if (check_phone_number_format(phone_number)) {
                    m_emergency_phone = phone_number;
                    std::stringstream ss;
                    ss << phone_number << " set as emergency phone number.";
                    SMSSender::instance().sendSMS(from, ss.str());
                    saveState();
                } else {
                    std::stringstream ss;
                    ss << "Phone number \"" << phone_number << "\" is not valid. Phone numbers must follow this format: (country code)(9-10 digits). Example: 3310203040506";
                    SMSSender::instance().sendSMS(from, ss.str());
                }
            }
        } else if (content == "REMOVE EMERGENCY PHONE") {
            if (!m_emergency_phone.empty()) {
                Logger::info("Removed emergency phone");
                SMSSender::instance().sendSMS(from, "Emergency phone removed");
            }
            m_emergency_phone.clear();
        } else if (content.rfind("HELP") == 0) {
            std::stringstream ss;
            ss << "Basic commands:\n";
            ss << "ALL OFF\n";
            ss << "ALL ECO\n";
            ss << "ALL COMFORT\n";
            ss << "ALL DEFROST\n";
            SMSSender::instance().sendSMS(from, ss.str());
        } else if (content == "DEBUG FILESTATE") {
            std::ifstream file(STATE_FILE_PATH);
            std::string line;
            std::stringstream msg;
            while(std::getline(file, line)) {
                msg << line << '\n';
            }
            SMSSender::instance().sendSMS(from, msg.str());
        } else if (content == "DEBUG STATE") {
            std::stringstream msg;
            switch (m_heater_default_state) {
            case HEATER_OFF: msg << "DEFAULT: OFF\n"; break;
            case HEATER_DEFROST: msg << "DEFAULT: DEFROST\n"; break;
            case HEATER_ECO: msg << "DEFAULT: ECO\n"; break;
            case HEATER_COMFORT: msg << "DEFAULT: COMFORT/ON\n"; break;
            }

            for (auto &e : m_heater_state) {
                switch (e.second) {
                case HEATER_OFF: msg << "HEATER " << e.first << ": OFF\n"; break;
                case HEATER_DEFROST: msg << "HEATER " << e.first << ": DEFROST\n"; break;
                case HEATER_ECO: msg << "HEATER " << e.first << ": ECO\n"; break;
                case HEATER_COMFORT: msg << "HEATER " << e.first << ": COMFORT/ON\n"; break;
                }
            }
            SMSSender::instance().sendSMS(from, msg.str());
        } else if (content == "DEBUG REBOOT") {
            sync();
            reboot(RB_AUTOBOOT);
        } else if (content == "DEBUG WIFI") {
            std::array<char, 512> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("iwconfig wlan0", "r"), pclose);
            if (!pipe) {
                SMSSender::instance().sendSMS(from, "Fail to get wifi connection info");
            } else {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
                    result += buffer.data();
                if (result.size() > 512)
                    result.resize(512);
                if (result.empty())
                    SMSSender::instance().sendSMS(from, "Unable to get wifi connection info");
                else
                    SMSSender::instance().sendSMS(from, result);
            }
        } else if (content == "DEBUG LOG") {
            /* Read logs from base_station.service */
            std::array<char, 4096> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("journalctl --unit=basestation.service --no-pager", "r"), pclose);
            if (!pipe) {
                SMSSender::instance().sendSMS(from, "Fail to get basestation logs");
            } else {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
                    result += buffer.data();
                if (result.empty()) {
                    SMSSender::instance().sendSMS(from, "Unable to get basestation logs");
                } else {
                    std::string msg;
                    while (!result.empty()) {
                        msg = result.substr(0, 512);
                        result.erase(0, 512);
                        SMSSender::instance().sendSMS(from, msg);
                    }
                }
            }
        } else {
            std::stringstream ss;
            ss << "Received invalid message from: " << from;
            Logger::warn(ss.str());
        }
    }
}

void BaseStation::sendVersion(const std::string &to)
{
    std::stringstream content;

    content << "base_station-" << GIT_HASH << '.' << BUILD_TIME;

    SMSSender::instance().sendSMS(to, content.str());
}

void BaseStation::checkStaleConnections()
{
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
    auto itor = m_connections.begin();
    while (itor != m_connections.end()) {
        if (std::chrono::duration_cast<std::chrono::minutes>(t - itor->last_seen) > std::chrono::minutes(30)) {
            Logger::info("Removing stale connection");
            close(itor->fd);
            itor = m_connections.erase(itor);
        } else {
            ++itor;
        }
    }
}

void BaseStation::sendHeaterState(int fd, const std::string &name)
{
    message_header_t header;
    header.version = 1;
    header.type = MessageType::HEATER_STATE_REPLY;
    {
        struct ifreq s;
        int fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            Logger::warn("Unable to get MAC address of network interface " NETWORK_INTERFACE_NAME);
            memset(header.mac_addr, 0, sizeof(header.mac_addr));
        } else {
            strcpy(s.ifr_name, NETWORK_INTERFACE_NAME);
            if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
                for (int i = 0; i < 6; ++i)
                    header.mac_addr[i] = s.ifr_addr.sa_data[i];
            } else {
                Logger::warn("Unable to get MAC address of network interface " NETWORK_INTERFACE_NAME);
                memset(header.mac_addr, 0, sizeof(header.mac_addr));
            }
            close(fd);
        }
    }
    header.counter = m_message_counter++;

    uint8_t data[MESSAGE_SIZE];
    memset(data, 0xFF, sizeof(data));
    memcpy(data, &header, sizeof(header));

    auto it = m_heater_state.find(name);
    if (it != m_heater_state.end())
        data[sizeof(header)] = it->second;
    else
        data[sizeof(header)] = m_heater_default_state;

    int sent = 0;
    while (sent < MESSAGE_SIZE) {
        int ret = write(fd, &data[sent], MESSAGE_SIZE - sent);
        if (ret <= 0)
            break;
        sent += ret;
    }
}

void BaseStation::checkWifi()
{
    struct pollfd fds[1];

    fds[0].fd = m_check_wifi_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret > 0) {
        /* Dummy read with timer fd to clear event */
        uint64_t _;
        read(fds[0].fd, &_, sizeof(_));

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, NETWORK_INTERFACE_NAME);

        int dummy_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (ioctl(dummy_fd, SIOCGIFFLAGS, &ifr) != -1) {
            bool up_and_running = (ifr.ifr_flags & ( IFF_UP | IFF_RUNNING )) == ( IFF_UP | IFF_RUNNING );

            if (!up_and_running) {
                ++m_wifi_not_good_counter;
                if (m_wifi_not_good_counter == 15) {
                    Logger::err("Lost WiFi connection for past 15 minutes");

                    if (!m_emergency_phone.empty())
                        SMSSender::instance().sendSMS(m_emergency_phone, "Error! Base station lost WiFi connection for past 15 minutes. Heaters cannot be controlled.");
                }
            } else {
                if (m_wifi_not_good_counter) {
                    Logger::info("WiFi connection restored");
                    if (!m_emergency_phone.empty())
                        SMSSender::instance().sendSMS(m_emergency_phone, "Base station restored WiFi connection. System is now running ok.");
                }
                m_wifi_not_good_counter = 0;
            }
        } else {
            Logger::err("Cannot check connection status of network interface " NETWORK_INTERFACE_NAME);
        }
        close(dummy_fd);
    }
}

bool BaseStation::loadState()
{
    std::ifstream file(STATE_FILE_PATH);
    if (!file) {
        Logger::err("Could not load state from file " STATE_FILE_PATH);
        return false;
    }

    std::string line;
    while(std::getline(file, line)) {
        size_t ret = line.find('=');
        if (ret == std::string::npos)
            continue;
        std::string key = line.substr(0, ret);
        std::string val = line.substr(ret + 1);

        /* Trim name and val */
        key.erase(key.begin(), std::find_if(key.begin(), key.end(),
            [] (unsigned char c){ return !std::isspace(c); }));
        key.erase(std::find_if(key.rbegin(), key.rend(),
            [] (unsigned char c){ return !std::isspace(c); }).base(), key.end());
        val.erase(val.begin(), std::find_if(val.begin(), val.end(),
            [] (unsigned char c){ return !std::isspace(c); }));
        val.erase(std::find_if(val.rbegin(), val.rend(),
            [] (unsigned char c){ return !std::isspace(c); }).base(), val.end());
        if (key == "default_heater_state") {
            if (val == "off")
                m_heater_default_state = HEATER_OFF;
            else if (val == "defrost")
                m_heater_default_state = HEATER_DEFROST;
            else if (val == "eco")
                m_heater_default_state = HEATER_ECO;
            else if (val == "comfort")
                m_heater_default_state = HEATER_COMFORT;
            else {
                m_heater_default_state = HEATER_DEFROST;
                Logger::err("Invalid value for default_heater_state key. Setting default_heater_state to DEFROST.");
            }
        } else if (key.rfind("heater_", 0) == 0
                && ends_with(key, "_state")) {
            std::string name = key.substr(7, key.length() - 7 - 6);

            if (check_heater_name(name)) {
                for (unsigned int i = 0; i < name.length(); ++i)
                    name[i] = toupper(name[i]);

                if (val == "off")
                    m_heater_state[name] = HEATER_OFF;
                else if (val == "defrost")
                    m_heater_state[name] = HEATER_DEFROST;
                else if (val == "eco")
                    m_heater_state[name] = HEATER_ECO;
                else if (val == "comfort")
                    m_heater_state[name] = HEATER_COMFORT;
                else {
                    std::stringstream msg;
                    msg << "Invalid value \"" << val << "\" for heater " << name;
                    Logger::warn(msg.str());
                }
            } else {
                std::stringstream msg;
                msg << "Invalid heater name \"" << name << "\".";
                Logger::warn(msg.str());
            }
        } else if (key == "whitelist") {
            std::istringstream iss(val);
            std::string item;
            while (std::getline(iss, item, ',')) {
                if (check_phone_number_format(item)) {
                    m_phone_whitelist.insert(item);
                } else {
                    std::stringstream msg;
                    msg << "Invalid phone number: " << item;
                    Logger::warn(msg.str());
                }
            }
        } else if (key == "emergency_phone") {
            if (check_phone_number_format(val)) {
                m_emergency_phone = val;
            } else {
                std::stringstream msg;
                msg << "Invalid emergency phone number: " << val;
                Logger::warn(msg.str());
            }
        } else {
            std::stringstream ss;
            ss << "Invalid key \"" << key << '\"';
            Logger::warn(ss.str());
        }
    }

    Logger::debug("Loaded state from file " STATE_FILE_PATH);

    return true;
}

void BaseStation::saveState()
{
    std::ofstream file(STATE_FILE_PATH);
    if (!file) {
        Logger::err("Could not save state to file " STATE_FILE_PATH);
        return;
    }
    switch (m_heater_default_state) {
    case HEATER_OFF:
        file << "default_heater_state=off\n";
        break;
    case HEATER_DEFROST:
        file << "default_heater_state=defrost\n";
        break;
    case HEATER_ECO:
        file << "default_heater_state=eco\n";
        break;
    case HEATER_COMFORT:
        file << "default_heater_state=comfort\n";
        break;
    }

    for (auto &e : m_heater_state) {
        switch (e.second) {
        case HEATER_OFF:
            file << "heater_" << e.first << "_state=off\n";
            break;
        case HEATER_DEFROST:
            file << "heater_" << e.first << "_state=defrost\n";
            break;
        case HEATER_ECO:
            file << "heater_" << e.first << "_state=eco\n";
            break;
        case HEATER_COMFORT:
            file << "heater_" << e.first << "_state=comfort\n";
            break;
        }
    }

    file << "whitelist=";
    auto itor = m_phone_whitelist.begin();
    while (itor != m_phone_whitelist.end()) {
        file << *itor;
        ++itor;
        if (itor != m_phone_whitelist.end())
            file << ',';
    }
    file << '\n';

    file << "emergency_phone=" << m_emergency_phone << '\n';

    Logger::debug("Saved state to file " STATE_FILE_PATH);
}
