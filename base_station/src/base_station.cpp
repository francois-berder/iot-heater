#include "base_station.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include "version.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
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
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#ifndef BASE_STATION_PIN
#define BASE_STATION_PIN    "1234"
#endif

#define MESSAGE_SIZE    (64)

#define STATE_FILE_PATH     "/var/lib/base_station.state"

#define CHECK_WIFI_PERIOD           (60 * 1000)         /* in milliseconds */
#define WIFI_ERROR_THRESHOLD        (15)
#define CHECK_LOST_DEVICES_PERIOD   (60 * 60 * 1000)    /* in milliseconds */
#define NETWORK_INTERFACE_NAME      "wlan0"
#define DEVICE_LOST_THRESHOLD       (24 * 60 * 60)  /* in seconds */
#define CHECK_3G_PERIOD             (5 * 60 * 1000)     /* in milliseconds */
#define MODULE_3G_ERROR_THRESHOLD   (6)
#define FALLBACK_HEATER_STATE       (HEATER_DEFROST)
#define CHECK_DAEMON_PERIOD         (5 * 60 * 1000)     /* in milliseconds */
#define DAEMON_ERROR_THRESHOLD      (6)
#define SEND_BOOT_MSG_PERIOD        (30 * 1000)

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

std::stringstream& macToStr(std::stringstream &ss, uint8_t mac[6])
{
    char buf[32];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);
    ss << buf;
    return ss;
}

std::string get_uptime_str()
{
    struct sysinfo s_info;
    int ret = sysinfo(&s_info);
    if (ret)
        return "unknown";

    unsigned int secs = s_info.uptime;
    unsigned int days = secs / (60 * 60 * 24);
    secs -= days * (60 * 60 * 24);
    unsigned int hours = secs / (60 * 60);
    secs -= hours * (60 * 60);
    unsigned int minutes = secs / 60;
    secs -= minutes * 60;

    std::stringstream ss;
    ss << days << " days " << hours << "h " << minutes << "m " << secs << "s";
    return ss.str();
}

std::string get_machineinfo_str()
{
    struct utsname u;
    int ret = uname(&u);
    if (ret)
        return "unknown";

    std::stringstream ss;
    ss << u.sysname << '-' << u.nodename << '-' << u.release << '-' << u.version << '-' << u.machine;
    return ss.str();
}

bool check_3g_module_presence()
{
    /*
     * /dev/ttyUSB2 used by smstools daemon
     * /dev/ttyUSB3 another AT interface
     */
    return access("/dev/ttyUSB2", F_OK) == 0 && access("/dev/ttyUSB3", F_OK) == 0;
}

enum modem_status_t {
    MODEM_COMS_FAILURE,
    MODEM_SIM_ERROR,
    MODEM_NETWORK_NOT_REGISTERED,
    MODEM_NETWORK_DENIED,
    MODEM_NETWORK_REGISTERED,
    MODEM_NETWORK_SEARCHING,
    MODEM_NETWORK_UNKNOWN,
    MODEM_NETWORK_ROAMING,
};

enum modem_status_t check_3g_connection()
{
    int fd;
    struct termios old_params, new_params;
    enum modem_status_t status = MODEM_COMS_FAILURE;

    fd = open("/dev/ttyUSB3", O_RDWR);
    if (fd < 0)
        return status;

    /* Configure serial port */
    if (tcgetattr(fd, &old_params) < 0) {
        close(fd);
        return status;
    }

    if (tcgetattr(fd, &new_params) < 0)
        goto cleanup_serial;

    cfmakeraw(&new_params);
    new_params.c_lflag &= ~ICANON;
    new_params.c_cc[VMIN] = 0;
    new_params.c_cc[VTIME] = 1;
    if (cfsetispeed(&new_params, B9600)
     || cfsetospeed(&new_params, B9600)
     || tcsetattr(fd, TCSANOW, &new_params))
         goto cleanup_serial;

    tcflush(fd, TCIOFLUSH);

    /* Check 3G module */
    if (write(fd, "AT\r\n", 4) != 4)
        goto cleanup_serial;

    {
        std::string reply;
        while (1) {
            char buf[32];
            int ret = read(fd, buf, sizeof(buf) - 1);
            if (ret <= 0)
                break;
            buf[ret] = '\0';
            reply += buf;
        }

        if (reply.find("OK") == std::string::npos)
            goto cleanup_serial;
    }

    tcflush(fd, TCIOFLUSH);

    /* Check SIM card */
    if (write(fd, "AT+CPIN?\r\n", 10) != 10)
        goto cleanup_serial;

    {
        std::string reply;
        while (1) {
            char buf[32];
            int ret = read(fd, buf, sizeof(buf) - 1);
            if (ret <= 0)
                break;
            buf[ret] = '\0';
            reply += buf;
        }

        if (reply.find("+CPIN: READY") == std::string::npos)
            goto cleanup_serial;
    }

    tcflush(fd, TCIOFLUSH);

    /* Check connectivity */
    if (write(fd, "AT+CREG?\r\n", 10) != 10)
        goto cleanup_serial;

    {
        std::string reply;
        while (1) {
            char buf[32];
            int ret = read(fd, buf, sizeof(buf) - 1);
            if (ret <= 0)
                break;
            buf[ret] = '\0';
            reply += buf;
        }

        if (reply.find("+CREG: 0,0") != std::string::npos)
            status = MODEM_NETWORK_NOT_REGISTERED;
        else if (reply.find("+CREG: 0,1") != std::string::npos)
            status = MODEM_NETWORK_REGISTERED;
        else if (reply.find("+CREG: 0,2") != std::string::npos)
            status = MODEM_NETWORK_SEARCHING;
        else if (reply.find("+CREG: 0,3") != std::string::npos)
            status = MODEM_NETWORK_DENIED;
        else if (reply.find("+CREG: 0,4") != std::string::npos)
            status = MODEM_NETWORK_UNKNOWN;
        else if (reply.find("+CREG: 0,5") != std::string::npos)
            status = MODEM_NETWORK_ROAMING;
        else
            status = MODEM_NETWORK_UNKNOWN;
    }

cleanup_serial:
    tcsetattr(fd, TCSANOW, &old_params);
    close(fd);

    return status;
}

std::string get_ip_address_str()
{
    int fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return "unknown";

    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, NETWORK_INTERFACE_NAME, IFNAMSIZ-1);

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        close(fd);
        return "unknown";
    }

    close(fd);

    char buf[32];
    sprintf(buf, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    return std::string(buf);
}

/* From https://stackoverflow.com/questions/6898337/determine-programmatically-if-a-program-is-running */
bool is_process_running(const std::string &name)
{
    DIR* dir;
    struct dirent* ent;
    char buf[512];

    long pid;
    char pname[100] = {0,};
    char state;
    FILE *fp=NULL;

    if (!(dir = opendir("/proc"))) {
        Logger::err("can't open /proc");
        return false;
    }

    while((ent = readdir(dir)) != NULL) {
        long lpid = atol(ent->d_name);
        if(lpid < 0)
            continue;
        snprintf(buf, sizeof(buf), "/proc/%ld/stat", lpid);
        fp = fopen(buf, "r");

        if (fp) {
            if ( (fscanf(fp, "%ld (%[^)]) %c", &pid, pname, &state)) != 3 ){
                fclose(fp);
                closedir(dir);
                return false;
            }
            if (!strcmp(pname, name.c_str())) {
                fclose(fp);
                closedir(dir);
                return true;
            }
            fclose(fp);
        }
    }

    closedir(dir);
    return false;
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
m_wifi_error_counter(0),
m_message_counter(0),
m_heater_counter(),
m_heaters(),
m_heaters_mutex(),
m_lost_devices_timer(),
m_check_3g_timer(),
m_3g_error_counter(0),
m_check_daemon_timer(),
m_daemon_error_counter(0),
m_send_boot_msg()
{
    if (!loadState())
        saveState();

    /* Start check wifi timer */
    m_check_wifi_timer.start(CHECK_WIFI_PERIOD, true);

    /* Start lost devices timer */
    m_lost_devices_timer.start(CHECK_LOST_DEVICES_PERIOD, true);

    /* Start check 3G timer */
    m_check_3g_timer.start(CHECK_3G_PERIOD, true);

    /* Start check daemon timer */
    m_check_daemon_timer.start(CHECK_DAEMON_PERIOD, true);

    /* Start send boot msg timer */
    m_send_boot_msg.start(SEND_BOOT_MSG_PERIOD, false);

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
    parseCommands();
    checkStaleConnections();
    checkWifi();
    checkLostDevices();
    check3G();
    checkSMSDaemon();
    sendBootMsg();
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

std::string BaseStation::buildWebpage()
{
    std::stringstream ss;

    ss << "<html><head>\
        <style>\
        table, td, th {\
        border: 1px solid black;\
        }\
        table {\
        width: 100%;\
        border-collapse: collapse;\
        }\
        </style>\
        <title>Base station</title>\
        </head><body>";

    ss << "<h1>Base station</h1>";
    ss << "<h2>Device information</h2>";
    ss << "Software version: " << get_version_str();
    ss << "<br>";
    ss << "Uptime: " << get_uptime_str();
    ss << "<br>";
    ss << "Machine info: " << get_machineinfo_str();
    ss << "<br>";
    ss << "IP address: " << get_ip_address_str();
    ss << "<br>";
    bool modem_detected = check_3g_module_presence();
    ss << "3G module detected: " << (modem_detected ? "yes" : "<span style=\"color:red\">no</span>");
    ss << "<br>";
    if (modem_detected) {
        ss << "3G module network status: ";
        enum modem_status_t status = check_3g_connection();
        switch (status) {
        case MODEM_COMS_FAILURE: ss << "<span style=\"color:red\">comms failure</span>"; break;
        case MODEM_SIM_ERROR: ss << "<span style=\"color:red\">SIM card error</span>"; break;
        case MODEM_NETWORK_NOT_REGISTERED: ss << "<p style=\"color:red\">not registered</span>"; break;
        case MODEM_NETWORK_DENIED: ss << "<span style=\"color:red\">denied</span>"; break;
        case MODEM_NETWORK_REGISTERED: ss << "OK"; break;
        case MODEM_NETWORK_SEARCHING: ss << "connecting"; break;
        case MODEM_NETWORK_UNKNOWN: ss << "<span style=\"color:red\">unknown</span>"; break;
        case MODEM_NETWORK_ROAMING: ss << "roaming"; break;
        default: ss << "<span style=\"color:red\">cannot get network status</span>"; break;
        }
    }
    ss << "<br>";
    if (is_process_running("smsd"))
        ss << "SMS daemon: running";
    else
        ss << "SMS daemon: <span style=\"color:red\">not running</span>";
    ss << "<h2>Heaters</h2>";
    ss << "Default heater state: ";
    switch (m_heater_default_state) {
    case HEATER_OFF: ss << "OFF"; break;
    case HEATER_DEFROST: ss << "DEFROST"; break;
    case HEATER_ECO: ss << "ECO"; break;
    case HEATER_COMFORT: ss << "COMFORT/ON"; break;
    default: ss << "<span style=\"color:red\">UNKNOWN</span>"; break;
    }
    ss << "<br>";

    ss << "<table>";
    ss << "<tr>";
    ss << "<th>Name</th>";
    ss << "<th>MAC address</th>";
    ss << "<th>IP address</th>";
    ss << "<th>State</th>";
    ss << "<th>Last request timestamp</th>";
    ss << "</tr>";

    {
        std::lock_guard<std::mutex> guard(m_heaters_mutex);

    for (auto it : m_heaters) {
        uint64_t mac = it.first;
        Heater h = it.second;

        ss << "<tr>";
        if (!h.getName().empty())
            ss << "<td>" << h.getName() << "</td>";
        else
            ss << "<td>?</td>";

        {
            char buf[32];
            sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
                    (uint8_t)((mac >> 40) & 0xFF),
                    (uint8_t)((mac >> 32) & 0xFF),
                    (uint8_t)((mac >> 24) & 0xFF),
                    (uint8_t)((mac >> 16) & 0xFF),
                    (uint8_t)((mac >> 8) & 0xFF),
                    (uint8_t)((mac & 0xFF)));
            ss << "<td>" << buf << "</td>";
        }

        ss << "<td>" << h.getLastIPAddress() << "</td>";

        switch (h.getState()) {
        case HEATER_OFF: ss << "<td>OFF</td>"; break;
        case HEATER_DEFROST: ss << "<td>DEFROST</td>"; break;
        case HEATER_ECO: ss << "<td>ECO</td>"; break;
        case HEATER_COMFORT: ss << "<td>COMFORT/ON</td>"; break;
        default: ss << "<td><span style=\"color:red\">UNKNOWN</span></td>"; break;
        }

        {
            time_t ts = h.getLastRequestTimestamp();
            char buf[128];
            strftime(buf, sizeof(buf) - 1, "%d-%m-%Y %H:%M:%S", localtime(&ts));
            ss << "<td>" << buf << "</td>";
        }

        ss << "</tr>";
    }
    }

    ss << "</table>";

    ss << "</body></html>";

    return ss.str();
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

void BaseStation::checkStaleConnections()
{
    struct pollfd fds[1];

    fds[0].fd = m_stale_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret > 0) {
        /* Dummy read with timer fd to clear event */
        uint64_t _;
        read(fds[0].fd, &_, sizeof(_));

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

    uint64_t mac_addr;
    mac_addr = ((uint64_t)header.mac_addr[0] << 40LU)
            | ((uint64_t)header.mac_addr[1] << 32LU)
            | ((uint64_t)header.mac_addr[2] << 24LU)
            | ((uint64_t)header.mac_addr[3] << 16LU)
            | ((uint64_t)header.mac_addr[4] << 8LU)
            | ((uint64_t)header.mac_addr[5]);

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

            if (!name.empty()) {
                if (!check_heater_name(name)) {
                    std::stringstream ss;
                    ss << "Invalid name parameter in HEATER_STATE_REQ from device ";
                    macToStr(ss, header.mac_addr);
                    Logger::err(ss.str());
                    name.clear();
                }
            }
        }

        {
            std::stringstream ss;
            ss << "Received heater state request from device ";
            if (!name.empty())
                ss << name << " ";
            ss << "MAC=";
            macToStr(ss, header.mac_addr);
            Logger::debug(ss.str());
        }

        /* Check if device rebooted since last message */
        auto it = m_heater_counter.find(mac_addr);
        if (it != m_heater_counter.end()
        &&  (uint64_t)(header.counter - it->second) > 3LLU) {
            {
                std::stringstream ss;
                ss << "It seems that device ";
                if (!name.empty())
                    ss << name << " MAC=";
                macToStr(ss, header.mac_addr) << " rebooted a few minutes ago";
                Logger::warn(ss.str());
            }
            {
                std::stringstream ss;
                ss << "Warning!\nDevice ";
                if (!name.empty())
                    ss << name << " MAC=";
                macToStr(ss, header.mac_addr);
                ss << " probably rebooted a few minutes ago.",
                SMSSender::instance().sendSMS(m_emergency_phone, ss.str());
            }
        }
        m_heater_counter[mac_addr] = header.counter;

        HeaterState state = m_heater_default_state;
        if (!name.empty()) {
            auto it = m_heater_state.find(name);
            if (it != m_heater_state.end())
                state = it->second;
        }
        {
            struct sockaddr_in addr;
	        socklen_t len = sizeof(addr);
            getpeername(conn.fd, (struct sockaddr *)&addr, &len);
            char dst[32];
            inet_ntop(AF_INET, &addr.sin_addr, dst, sizeof(dst));
            std::lock_guard<std::mutex> guard(m_heaters_mutex);
            m_heaters[mac_addr] = Heater(name, dst);
            m_heaters[mac_addr].update(state);
        }
        sendHeaterState(conn.fd, state);
    } else if (header.type == MessageType::HEATER_STATE_REPLY) {
        std::stringstream ss;
        ss << "Ignoring HEATER_STATE_REPLY message from device ";

        {
            std::lock_guard<std::mutex> guard(m_heaters_mutex);

            auto it = m_heaters.find(mac_addr);
            if (it != m_heaters.end()) {
                const std::string &name = it->second.getName();
                if (!name.empty())
                    ss << name << " ";
            }
        }
        macToStr(ss, header.mac_addr);
        Logger::err(ss.str());
    } else {
        std::stringstream ss;
        ss << "Received unknown message type " << header.type << " from device ";
        {
            std::lock_guard<std::mutex> guard(m_heaters_mutex);

            auto it = m_heaters.find(mac_addr);
            if (it != m_heaters.end()) {
                const std::string &name = it->second.getName();
                if (!name.empty())
                    ss << name << " ";
            }
        }
        macToStr(ss, header.mac_addr);
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

            /*
             * Silently drop the message to avoid a
             * denial of service attack. Otherwise, an attacker
             * could make the base station sends tons of SMS
             * which could cost lots of money.
             */
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
        } else if (content == "DEBUG UPTIME") {
            SMSSender::instance().sendSMS(from, get_uptime_str());
        } else {
            std::stringstream ss;
            ss << "Received invalid message from: " << from;
            Logger::warn(ss.str());
            SMSSender::instance().sendSMS(from, "Received invalid command");
        }
    }
}

void BaseStation::sendVersion(const std::string &to)
{
    SMSSender::instance().sendSMS(to, get_version_str());
}

void BaseStation::sendHeaterState(int fd, HeaterState state)
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
    data[sizeof(header)] = state;

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
                ++m_wifi_error_counter;
                if (m_wifi_error_counter == WIFI_ERROR_THRESHOLD) {
                    unsigned int secs = (WIFI_ERROR_THRESHOLD * CHECK_WIFI_PERIOD) / 1000;
                    unsigned int hours = secs / 3600;
                    secs -= hours * 3600;
                    unsigned int mins = secs / 60;
                    secs -= mins * 60;
                    std::stringstream ss;
                    ss << "Lost WiFi connection for past " << hours << 'h' << mins << 'm' << secs << 's';
                    Logger::err(ss.str());

                    if (!m_emergency_phone.empty()) {
                        std::stringstream ss;
                        ss << "Error! Base station lost WiFi connection for past ";
                        ss << hours << 'h' << mins << 'm' << secs << "s. ";
                        ss << "Heaters cannot be controlled (they will switch to DEFROST mode automatically).";
                        SMSSender::instance().sendSMS(m_emergency_phone, ss.str());
                    }
                }
            } else {
                if (m_wifi_error_counter) {
                    Logger::info("WiFi connection restored");
                    if (!m_emergency_phone.empty())
                        SMSSender::instance().sendSMS(m_emergency_phone, "Base station restored WiFi connection. System is now running ok.");
                }
                m_wifi_error_counter = 0;
            }
        } else {
            Logger::err("Cannot check connection status of network interface " NETWORK_INTERFACE_NAME);
        }
        close(dummy_fd);
    }
}

void BaseStation::checkLostDevices()
{
    struct pollfd fds[1];

    fds[0].fd = m_lost_devices_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret <= 0)
        return;

    /* Dummy read with timer fd to clear event */
    uint64_t _;
    read(fds[0].fd, &_, sizeof(_));

    time_t now = time(NULL);
    std::map<uint64_t, std::string> lost_devices;
    {
        std::lock_guard<std::mutex> guard(m_heaters_mutex);
        auto it = m_heaters.begin();
        while (it != m_heaters.end()) {
            if (now - it->second.getLastRequestTimestamp() < DEVICE_LOST_THRESHOLD) {
                ++it;
                continue;
            }

            lost_devices[it->first] = it->second.getName();
            it = m_heaters.erase(it);
        }
    }

    for (auto it : lost_devices) {
        uint64_t mac = it.first;
        const std::string &name = it.second;

        uint8_t mac_addr[6];
        mac_addr[0] = mac >> 40;
        mac_addr[1] = mac >> 32;
        mac_addr[2] = mac >> 24;
        mac_addr[3] = mac >> 16;
        mac_addr[4] = mac >> 8;
        mac_addr[5] = mac;

        std::stringstream ss;
        ss << "Did not receive valid message from device ";
        if (!name.empty())
            ss << name << ' ';
        ss << "MAC=";
        macToStr(ss, mac_addr);
        ss << " for more than ";

        unsigned int secs = DEVICE_LOST_THRESHOLD;
        unsigned int hours = secs / (60 * 60);
        secs -= hours * 60 * 60;
        unsigned int mins = secs / 60;
        secs -= mins * 60;
        ss << hours << 'h' << mins << 'm' << secs << 's',
        Logger::warn(ss.str());
    }

    if (!m_emergency_phone.empty() && !lost_devices.empty()) {
        std::stringstream ss;
        if (lost_devices.size() > 1)
            ss << "WARNING! Lost connection with %d devices: ";
        else
            ss << "WARNING! Lost connection with one device: ";

        auto it = lost_devices.begin();
        while (it != lost_devices.end()) {
            const std::string &name = it->second;
            if (!name.empty()) {
                ss << name;
            } else {
                uint64_t mac = it->first;
                uint8_t mac_addr[6];
                mac_addr[0] = mac >> 40;
                mac_addr[1] = mac >> 32;
                mac_addr[2] = mac >> 24;
                mac_addr[3] = mac >> 16;
                mac_addr[4] = mac >> 8;
                mac_addr[5] = mac;
                macToStr(ss, mac_addr);
            }
 
            ++it;
            if (it != lost_devices.end())
                ss << ", ";
        }

        SMSSender::instance().sendSMS(m_emergency_phone, ss.str());
    }
}

void BaseStation::check3G()
{
    struct pollfd fds[1];

    fds[0].fd = m_check_3g_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret <= 0)
        return;

    /* Dummy read with timer fd to clear event */
    uint64_t _;
    read(fds[0].fd, &_, sizeof(_));

    if (!check_3g_module_presence()) {
        ++m_3g_error_counter;
        if (m_3g_error_counter == MODULE_3G_ERROR_THRESHOLD) {
            unsigned int secs = (CHECK_3G_PERIOD * MODULE_3G_ERROR_THRESHOLD) / 1000;
            unsigned int hours = secs / 3600;
            secs -= hours * 3600;
            unsigned int mins = secs / 60;
            secs -= mins * 60;

            {
                std::stringstream ss;
                ss << "3G module not detected for the last " << hours<< 'h' << mins << 'm' << secs << 's';
                Logger::err(ss.str());
            }

            m_heater_default_state = FALLBACK_HEATER_STATE;
            for (auto &it : m_heater_state)
                it.second = FALLBACK_HEATER_STATE;

            {
                std::stringstream ss;
                ss << "Setting all heaters to ";
                switch (FALLBACK_HEATER_STATE) {
                case HEATER_OFF: ss << "OFF"; break;
                case HEATER_DEFROST: ss << "DEFROST"; break;
                case HEATER_ECO: ss << "ECO"; break;
                case HEATER_COMFORT: ss << "COMFORT/ON"; break;
                default: ss << "UNKNOWN"; break;
                }
                ss << " mode due to 3G module not being detected.";
                Logger::info(ss.str());
            }
        }
    } else {
        enum modem_status_t status = check_3g_connection();
        if (status == MODEM_NETWORK_REGISTERED || status == MODEM_NETWORK_ROAMING) {
            if (m_3g_error_counter >= MODULE_3G_ERROR_THRESHOLD) {
                std::stringstream ss;
                ss << "INFO! All heaters were set to ";
                switch (FALLBACK_HEATER_STATE) {
                case HEATER_OFF: ss << "OFF"; break;
                case HEATER_DEFROST: ss << "DEFROST"; break;
                case HEATER_ECO: ss << "ECO"; break;
                case HEATER_COMFORT: ss << "COMFORT/ON"; break;
                default: ss << "UNKNOWN"; break;
                }

                ss << " mode due to earlier 3G module errors";
                Logger::info(ss.str());
                SMSSender::instance().sendSMS(m_emergency_phone, ss.str());
            }

            m_3g_error_counter = 0;
        }
    }
}

void BaseStation::checkSMSDaemon()
{
    struct pollfd fds[1];

    fds[0].fd = m_check_daemon_timer.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret <= 0)
        return;

    /* Dummy read with timer fd to clear event */
    uint64_t _;
    read(fds[0].fd, &_, sizeof(_));

    /* Check that smsd is running */
    if (!is_process_running("smsd")) {
        m_daemon_error_counter++;
        if (m_daemon_error_counter == DAEMON_ERROR_THRESHOLD) {
            unsigned int secs = (CHECK_DAEMON_PERIOD * DAEMON_ERROR_THRESHOLD) / 1000;
            unsigned int hours = secs / 3600;
            secs -= hours * 3600;
            unsigned int mins = secs / 60;
            secs -= mins * 60;

            {
                std::stringstream ss;
                ss << "smstools not running for the last " << hours<< 'h' << mins << 'm' << secs << 's';;
                Logger::err(ss.str());
            }

            m_heater_default_state = FALLBACK_HEATER_STATE;
            for (auto &it : m_heater_state)
                it.second = FALLBACK_HEATER_STATE;

            {
                std::stringstream ss;
                ss << "Setting all heaters to ";
                switch (FALLBACK_HEATER_STATE) {
                case HEATER_OFF: ss << "OFF"; break;
                case HEATER_DEFROST: ss << "DEFROST"; break;
                case HEATER_ECO: ss << "ECO"; break;
                case HEATER_COMFORT: ss << "COMFORT/ON"; break;
                default: ss << "UNKNOWN"; break;
                }
                ss << " mode due to SMS daemon not running.";
                Logger::info(ss.str());
            }
        }
    } else {
        if (m_daemon_error_counter) {
                std::stringstream ss;
                ss << "INFO! All heaters were set to ";
                switch (FALLBACK_HEATER_STATE) {
                case HEATER_OFF: ss << "OFF"; break;
                case HEATER_DEFROST: ss << "DEFROST"; break;
                case HEATER_ECO: ss << "ECO"; break;
                case HEATER_COMFORT: ss << "COMFORT/ON"; break;
                default: ss << "UNKNOWN"; break;
                }

                ss << " mode due to earlier 3G module errors";
                Logger::info(ss.str());
                if (!m_emergency_phone.empty())
                    SMSSender::instance().sendSMS(m_emergency_phone, ss.str());
        }
        m_daemon_error_counter = 0;
    }
}

void BaseStation::sendBootMsg()
{
    struct pollfd fds[1];

    fds[0].fd = m_send_boot_msg.getFD();
    fds[0].events = POLLIN;

    int ret = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
    if (ret <= 0)
        return;

    /* Dummy read with timer fd to clear event */
    uint64_t _;
    read(fds[0].fd, &_, sizeof(_));

    if (!m_emergency_phone.empty()) {
        std::stringstream msg;
        msg << "INFO! Base station software started\n";
        msg << "Default heater state: ";
        switch (m_heater_default_state) {
        case HEATER_OFF: msg << "OFF"; break;
        case HEATER_DEFROST: msg << "DEFROST"; break;
        case HEATER_ECO: msg << "ECO"; break;
        case HEATER_COMFORT: msg << "COMFORT/ON"; break;
        default: msg << "UNKNOWN"; break;
        }
        msg << '\n';
        for (auto it : m_heater_state) {
            msg << "Heater " << it.first << " state: ";
            switch (it.second) {
            case HEATER_OFF: msg << "OFF"; break;
            case HEATER_DEFROST: msg << "DEFROST"; break;
            case HEATER_ECO: msg << "ECO"; break;
            case HEATER_COMFORT: msg << "COMFORT/ON"; break;
            default: msg << "UNKNOWN"; break;
            }
            msg << '\n';
        }
        SMSSender::instance().sendSMS(m_emergency_phone, msg.str());
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
