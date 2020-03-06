#include "home_gateway.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string.h>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH        "development"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME      "unknown-time"
#endif

enum MessageType {
    ALIVE,
};

enum DeviceFeatureID {
    HEATER,
};

HomeGateway::~HomeGateway()
{
    /* Close all file descriptors */
    for (auto& d : m_devices)
        close(d.conn.fd);
    for (auto& conn : m_connections)
        close(conn.fd);
}

void HomeGateway::process()
{
    handleConnections();
    handleDevices();
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
    std::lock_guard<std::mutex> guard(m_connections_mutex);

    if (m_connections.empty())
        return;

    int nfds = m_connections.size();
    struct pollfd *fds = new struct pollfd[nfds];
    {
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

            uint8_t data[64];
            ret = read(fds[i].fd, data, sizeof(data));
            if (ret != sizeof(data))
                continue;
            parseMessage(fds[i].fd, data);
        }
    }

    delete[] fds;
}

void HomeGateway::handleDevices()
{
    int nfds = m_devices.size();
    struct pollfd *fds = new struct pollfd[nfds];
    {
        int i = 0;
        for (auto &d : m_devices) {
            fds[i].fd = d.conn.fd;
            fds[i].events = POLLIN;
            ++i;
        }
    }

    int ret = poll(fds, nfds, 0);
    if (ret > 0) {
        for (int i = 0; i < nfds; ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;

            uint8_t data[64];
            ret = read(fds[i].fd, data, sizeof(data));
            if (ret != sizeof(data))
                continue;
            parseMessage(fds[i].fd, data);
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
                checkStaleConnectionsAndDevices();
        }
    }
}

void HomeGateway::parseMessage(int fd, uint8_t *data)
{
    DeviceUID uid;
    uint16_t type;

    memcpy(uid.data(), data, uid.size());
    memcpy(&type, &data[6], sizeof(type));
    data += sizeof(uid) + sizeof(type);

    if (type == MessageType::ALIVE) {

        std::string name(reinterpret_cast<char*>(data), 32);

        /* Let's check if another device has this name */
        for (auto &d: m_devices) {
            if (d.name == name && d.conn.fd != fd && d.uid != uid) {
                std::stringstream ss;
                ss << "Device " << uid_to_string(d.uid) << " already has the same name";
                Logger::warn(ss.str());
            }
        }

        /* Do we already know this device ? */
        if (lookup_device(uid)) {
            for (auto &d : m_devices) {
                if (d.uid != uid)
                    continue;

                if (d.name != name) {
                    std::stringstream ss;
                    ss << "Device " << uid_to_string(d.uid) << " changes name to \"" << name << '\"';
                    Logger::info(ss.str());
                }
                d.name = name;
                d.conn.last_seen = std::chrono::steady_clock::now();
                break;
            }
        } else {
            /* Create a new device */
            Device d;
            d.conn.fd = fd;
            d.conn.last_seen = std::chrono::steady_clock::now();
            d.uid = uid;
            d.name = name;
            m_devices.push_back(d);

            /* Remove DeviceConnection from list */
            {
                std::lock_guard<std::mutex> guard(m_connections_mutex);

                for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
                    if (it->fd == fd) {
                        m_connections.erase(it);
                        break;
                    }
                }
            }

            std::stringstream ss;
            ss << "New device. uid: " << uid_to_string(uid) << ", name: " << name;
            Logger::info(ss.str());
        }
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
        else if (content == "LIST" || content == "ENUMERATE")
            sendDeviceList(from);
    }
}

void HomeGateway::sendVersion(const std::string &to)
{
    std::stringstream content;

    content << "homegateway-" << GIT_HASH << '.' << BUILD_TIME;

    SMSSender::instance().sendSMS(to, content.str());
}

void HomeGateway::sendDeviceList(const std::string &to)
{
    std::stringstream content;

    unsigned int device_count = 0;
    std::list<std::string> device_names;

    for (auto &d : m_devices)
        device_names.push_back(d.name);

    if (device_count) {
        content << device_count << " devices registered\n";
        for (auto &n : device_names)
            content << n << '\n';
    } else {
        content << "No device registered\n";
    }

    SMSSender::instance().sendSMS(to, content.str());
}

bool HomeGateway::lookup_device(DeviceUID uid)
{
    for (auto &d : m_devices) {
        if (d.uid == uid)
            return true;
    }

    return false;
}

void HomeGateway::checkStaleConnectionsAndDevices()
{
    std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> guard(m_connections_mutex);
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

    {
        auto itor = m_devices.begin();
        while (itor != m_devices.end()) {
            if (std::chrono::duration_cast<std::chrono::minutes>(t - itor->conn.last_seen) > std::chrono::minutes(30)) {
                std::stringstream ss;
                ss << "Removing stale device, uid: " << uid_to_string(itor->uid) << ", name: " << itor->name;
                Logger::info(ss.str());
                close(itor->conn.fd);
                itor = m_devices.erase(itor);
            } else {
                ++itor;
            }
        }
    }
}
