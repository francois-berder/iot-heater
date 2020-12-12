#include "base_station.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string.h>
#include <sys/ioctl.h>
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
}

enum MessageType {
    ALIVE,
    HEATER,
};

BaseStation::BaseStation():
m_connections(),
m_connections_mutex(),
m_commands(),
m_commands_mutex(),
m_stale_timer(),
m_heater_state(HEATER_OFF),
m_heater_state_history()
{
    if (!loadState())
        saveState();
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
    uint16_t type;
    memcpy(&type, data, sizeof(type));
    data += sizeof(type);

    if (type == MessageType::ALIVE) {
        conn.last_seen = std::chrono::steady_clock::now();
        sendHeaterState(conn.fd);
    } else {
        std::stringstream ss;
        ss << "Received unknown message type " << type << " from ";
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
        if (locked && !m_phone_whitelist.empty() && m_phone_whitelist.find(from) == m_phone_whitelist.end()) {
            std::stringstream ss;
            ss << "Received SMS from phone number \"" << from << "\" not in whitelist";
            Logger::warn(ss.str());
            return;
        }

        /* Trim content */
        content.erase(content.begin(), std::find_if(content.begin(), content.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
        content.erase(std::find_if(content.rbegin(), content.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), content.end());

        /* Convert all lowercase characters to uppercase */
        for (auto & c: content) c = toupper(c);

        if (content == "PING")
            SMSSender::instance().sendSMS(from, "PONG");
        else if (content == "DEBUG")
            SMSSender::instance().setVerboseLevel(SMS_SENDER_DEBUG);
        else if (content == "VERBOSE")
            SMSSender::instance().setVerboseLevel(SMS_SENDER_VERBOSE);
        else if (content == "QUIET")
            SMSSender::instance().setVerboseLevel(SMS_SENDER_QUIET);
        else if (content == "VERSION")
            sendVersion(from);
        else if (content == "HEATER OFF") {
            setHeaterState(HEATER_OFF);
            SMSSender::instance().sendSMS(from, "HEATER OFF");
        } else if (content == "HEATER ECO") {
            setHeaterState(HEATER_ECO);
            SMSSender::instance().sendSMS(from, "HEATER ECO");
        } else if (content == "HEATER DEFROST") {
            setHeaterState(HEATER_DEFROST);
            SMSSender::instance().sendSMS(from, "HEATER DEFROST");
        } else if (content == "HEATER COMFORT") {
            setHeaterState(HEATER_COMFORT);
            SMSSender::instance().sendSMS(from, "HEATER COMFORT");
        } else if (content == "HEATER ON") {
            setHeaterState(HEATER_COMFORT);
            SMSSender::instance().sendSMS(from, "HEATER ON");
        } else if (content == "GET HEATER") {
            std::string val;
            switch (m_heater_state) {
            case HEATER_OFF: val = "HEATER OFF"; break;
            case HEATER_DEFROST: val = "HEATER DEFROST"; break;
            case HEATER_ECO: val = "HEATER ECO"; break;
            case HEATER_COMFORT: val = "HEATER COMFORT"; break;
            default: val = "HEATER UNKNOWN"; break;
            }
            SMSSender::instance().sendSMS(from, val);
        } else if (content == "GET IP") {
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("dig +short myip.opendns.com @resolver1.opendns.com", "r"), pclose);
            if (!pipe) {
                SMSSender::instance().sendSMS(from, "Fail to execute dig");
            } else {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
                    result += buffer.data();
                if (result.size() > 64)
                    result.resize(64);
                SMSSender::instance().sendSMS(from, result);
            }
        } else if (content == "GET HEATER HISTORY") {
            std::stringstream ss;
            ss << "Last Heater commands:\n";
            for (auto &h : m_heater_state_history) {
                std::time_t now_c = std::chrono::system_clock::to_time_t(h.first);
                tm *ltm = localtime(&now_c);
                std::stringstream date;
                date << ltm->tm_mday
                    << "/"
                    << 1 + ltm->tm_mon
                    << "/"
                    << 1900 + ltm->tm_year
                    << " "
                    << 1 + ltm->tm_hour
                    << ":"
                    << 1 + ltm->tm_min
                    << ":"
                    << 1 + ltm->tm_sec;
                switch (h.second) {
                case HEATER_OFF: ss << " OFF\n"; break;
                case HEATER_DEFROST: ss << " DEFROST\n"; break;
                case HEATER_ECO: ss << " ECO\n"; break;
                case HEATER_COMFORT: ss << " COMFORT\n"; break;
                }
            }

            SMSSender::instance().sendSMS(from, ss.str());
        } else if (content == "LOCK") {
            if (m_phone_whitelist.find(from) != m_phone_whitelist.end()) {
                SMSSender::instance().sendSMS(from, "LOCKED");
                locked = true;
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
                    locked = false;
                } else {
                    SMSSender::instance().sendSMS(from, "Wrong PIN");
                }
            }
        } else if (content.rfind("ADD PHONE ", 0) == 0) {
            if (!locked) {
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
            if (!locked) {
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
        } else if (content.rfind("HELP") == 0) {
            std::stringstream ss;
            ss << "Basic commands:\n";
            ss << "HEATER OFF\n";
            ss << "HEATER ECO\n";
            ss << "HEATER COMFORT\n";
            ss << "HEATER DEFROST\n";
            SMSSender::instance().sendSMS(from, ss.str());
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

void BaseStation::sendHeaterState(int fd)
{
    uint8_t data[MESSAGE_SIZE];
    memset(data, 0xFF, sizeof(data));
    uint16_t type = HEATER;
    memcpy(data, &type, sizeof(type));
    data[2] = m_heater_state;

    int sent = 0;
    while (sent < MESSAGE_SIZE) {
        int ret = write(fd, &data[sent], MESSAGE_SIZE - sent);
        if (ret <= 0)
            break;
        sent += ret;
    }
}

void BaseStation::setHeaterState(enum HeaterState heater_state)
{
    if (m_heater_state == heater_state)
        return;
    m_heater_state = heater_state;

    switch (m_heater_state) {
    case HEATER_OFF:
        Logger::debug("Changing heater state to OFF");
        break;
    case HEATER_DEFROST:
        Logger::debug("Changing heater state to DEFROST");
        break;
    case HEATER_ECO:
        Logger::debug("Changing heater state to ECO");
        break;
    case HEATER_COMFORT:
        Logger::debug("Changing heater state to COMFORT");
        break;
    }
    m_heater_state_history.push_front(std::pair<std::chrono::system_clock::time_point,HeaterState>(std::chrono::system_clock::now(), m_heater_state));

    /* Limit history to last 16 items */
    while (m_heater_state_history.size() > 16)
        m_heater_state_history.pop_back();

    saveState();
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
            std::not1(std::ptr_fun<int, int>(std::isspace))));
        key.erase(std::find_if(key.rbegin(), key.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), key.end());
        val.erase(val.begin(), std::find_if(val.begin(), val.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
        val.erase(std::find_if(val.rbegin(), val.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), val.end());
        if (key == "heater_state") {
            if (val == "off")
                m_heater_state = HEATER_OFF;
            else if (val == "defrost")
                m_heater_state = HEATER_DEFROST;
            else if (val == "eco")
                m_heater_state = HEATER_ECO;
            else if (val == "comfort")
                m_heater_state = HEATER_COMFORT;
            else {
                m_heater_state = HEATER_OFF;
                Logger::err("Invalid value for heater_state key. Setting heater_state to OFF.");
            }
        } else if (key == "whitelist") {
            std::istringstream iss(val);
            std::string item;
            while (std::getline(iss, item, ',')) {
                if (check_phone_number_format(item))
                    m_phone_whitelist.insert(item);
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
    switch (m_heater_state) {
    case HEATER_OFF:
        file << "heater_state=off\n";
        break;
    case HEATER_DEFROST:
        file << "heater_state=defrost\n";
        break;
    case HEATER_ECO:
        file << "heater_state=eco\n";
        break;
    case HEATER_COMFORT:
        file << "heater_state=comfort\n";
        break;
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

    Logger::debug("Saved state to file " STATE_FILE_PATH);
}
