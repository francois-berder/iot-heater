#include "home_gateway.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <algorithm>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH        "development"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME      "unknown-time"
#endif

#define MESSAGE_SIZE    (64)

#define STATE_FILE_PATH     "/var/lib/home_gateway.state"

enum MessageType {
    ALIVE,
    HEATER,
};

HomeGateway::HomeGateway():
m_connections(),
m_connections_mutex(),
m_commands(),
m_commands_mutex(),
m_stale_timer(),
m_heater_state(HEATER_OFF)
{
    if (!loadState())
        saveState();
}

HomeGateway::~HomeGateway()
{
    /* Close all file descriptors */
    for (auto& conn : m_connections)
        close(conn.fd);
}

void HomeGateway::process()
{
    handleConnections();
    handleTimers();
    parseCommands();
}

/*
 * Beware this function is called from device_server context !
 * That is why we need a mutex around the use of m_devices.
 */
void HomeGateway::handleNewDevice(int fd)
{
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    DeviceConnection conn;
    conn.fd = fd;
    conn.last_seen = std::chrono::steady_clock::now();

    m_connections.push_back(conn);
}

/*
 * Beware this function is called from sms_server context !
 */
void HomeGateway::handleSMSCommand(const std::string &from, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_commands_mutex);
    m_commands.emplace(from, content);
}

void HomeGateway::handleConnections()
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
    if (ret > 0) {
        for (int i = 0; i < nfds; ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;

            size_t n;
            if (ioctl(fds[i].fd, FIONREAD, &n) < 0)
                continue;

            for (auto &conn : m_connections) {
                if (conn.fd != fds[i].fd)
                    continue;

                while (n > MESSAGE_SIZE) {
                    uint8_t data[MESSAGE_SIZE];
                    if (read(fds[i].fd, data, sizeof(data)) != sizeof(data))
                        break;
                    parseMessage(conn, data);
                    n -= MESSAGE_SIZE;
                }
                break;
            }
        }
    }

    delete[] fds;
}

void HomeGateway::handleTimers()
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

void HomeGateway::parseMessage(DeviceConnection &conn, uint8_t *data)
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

void HomeGateway::parseCommands()
{
    while (!m_commands.empty()) {
        std::string from;
        std::string content;

        std::tie(from, content) = m_commands.front();
        m_commands.pop();

        /* Trim content */
        content.erase(content.begin(), std::find_if(content.begin(), content.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
        content.erase(std::find_if(content.rbegin(), content.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), content.end());

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
            if (m_heater_state != HEATER_OFF) {
                Logger::debug("Changing heater state to OFF");
                m_heater_state = HEATER_OFF;
                broadcastHeaterState();
                saveState();
            }
        } else if (content == "HEATER ECO") {
            if (m_heater_state != HEATER_ECO) {
                Logger::debug("Changing heater state to ECO");
                m_heater_state = HEATER_ECO;
                broadcastHeaterState();
                saveState();
            }
        } else if (content == "HEATER DEFROST") {
            if (m_heater_state != HEATER_DEFROST) {
                Logger::debug("Changing heater state to DEFROST");
                m_heater_state = HEATER_DEFROST;
                broadcastHeaterState();
                saveState();
            }
        } else if (content == "HEATER COMFORT") {
            if (m_heater_state != HEATER_COMFORT) {
                Logger::debug("Changing heater state to COMFORT");
                m_heater_state = HEATER_COMFORT;
                broadcastHeaterState();
                saveState();
            }
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
        }
    }
}

void HomeGateway::sendVersion(const std::string &to)
{
    std::stringstream content;

    content << "homegateway-" << GIT_HASH << '.' << BUILD_TIME;

    SMSSender::instance().sendSMS(to, content.str());
}

void HomeGateway::checkStaleConnections()
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

void HomeGateway::broadcastHeaterState()
{
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    for (auto &conn : m_connections)
        sendHeaterState(conn.fd);
}

void HomeGateway::sendHeaterState(int fd)
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

bool HomeGateway::loadState()
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
        } else {
            std::stringstream ss;
            ss << "Invalid key \"" << key << '\"';
            Logger::warn(ss.str());
        }
    }

    Logger::debug("Loaded state from file " STATE_FILE_PATH);

    return true;
}

void HomeGateway::saveState()
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

    Logger::debug("Saved state to file " STATE_FILE_PATH);
}
