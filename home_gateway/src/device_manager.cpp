#include "device_manager.hpp"
#include "heater_feature.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH        "development"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME      "unknown-time"
#endif

enum MessageType {
    DEVICE_REGISTER,
};

enum DeviceFeatureID {
    HEATER,
};

DeviceManager::~DeviceManager()
{
    std::lock_guard<std::mutex> guard(m_devices_mutex);

    /* Close all file descriptors */
    for (auto& d : m_devices)
        close(d.first);
}

void DeviceManager::process()
{
    struct pollfd *fds;
    size_t nfds;

    {
        std::lock_guard<std::mutex> guard(m_devices_mutex);

        nfds = m_devices.size();
        fds = new pollfd[nfds];
        size_t i = 0;
        for (auto& d : m_devices) {
            fds[i].fd = d.first;
            fds[i].events = POLLIN;
            ++i;
        }
    }

    int ret = poll(fds, nfds, 100);
    if (ret < 0) {
        std::stringstream ss;
        ss << "Failed to poll in device_server error: " << ret;
        Logger::err(ss.str());
        return;
    }

    parseCommands();

    if (ret == 0) {
        return;
    }

    for (size_t i = 0; i < nfds; ++i) {
        if (!(fds[i].revents & POLLIN))
            continue;

        uint8_t buffer[200];
        ret = read(fds[i].fd, buffer, sizeof(buffer));
        if (ret < 0) {
            std::stringstream ss;
            ss << "Failed to read in device_server error: " << ret;
            Logger::err(ss.str());
            continue;
        }

        if (ret > 0)
            parseMessage(fds[i].fd, buffer[0], &buffer[1], ret - 1);
    }
}

/*
 * Beware this function is called from device_server context !
 * That is why we need a mutex around the use of m_devices.
 */
void DeviceManager::handleNewDevice(int fd)
{
    std::lock_guard<std::mutex> guard(m_devices_mutex);

    m_devices[fd] = std::shared_ptr<Device>(new Device);
}

/*
 * Beware this function is called from sms_server context !
 */
void DeviceManager::handleSMSCommand(const std::string &from, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_commands_mutex);
    m_commands.emplace(from, content);
}

void DeviceManager::parseMessage(int fd, uint8_t type, uint8_t *data, int len)
{
    if (type == MessageType::DEVICE_REGISTER) {
        /*
         * DEVICE_REGISTER layout:
         *
         * ID: 20 characters
         * name: 32 characters
         * feature list: 12 bytes
         */
        if (len != 64) {
            std::stringstream ss;
            ss << "Received malformed DEVICE_REGISTER len=" << len;
            Logger::warn(ss.str());
            return;
        }

        std::string id(reinterpret_cast<char*>(data), 20);
        std::string name(reinterpret_cast<char*>(&data[20]), 32);
        m_devices[fd]->setID(id);
        m_devices[fd]->setName(name);

        /* Parse feature list */
        /* For now, we assume we only deal with simple heater */
        if (data[52] == DeviceFeatureID::HEATER) {
            m_devices[fd]->addFeature(std::shared_ptr<DeviceFeature>(new HeaterFeature));
            saveToFile();
        } else {
            std::stringstream ss;
            ss << "Ignored invalid feature " << data[52];
            Logger::warn(ss.str());
        }
    }
}

void DeviceManager::parseCommands()
{
    std::lock_guard<std::mutex> guard(m_commands_mutex);

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

void DeviceManager::saveToFile()
{
    std::ofstream file;

    file.open("devices.csv", std::fstream::out | std::fstream::trunc);
    if (!file) {
        Logger::err("Could not save device info to devices.csv");
        return;
    }

    std::string content;
    {
        std::lock_guard<std::mutex> guard(m_devices_mutex);

        for (auto& d : m_devices) {
            if (!d.second->isRegistered())
                continue;
            content += d.second->serialize();
            content += '\n';
        }
    }

    file << content;
}

void DeviceManager::sendVersion(const std::string &to)
{
    std::stringstream content;

    content << "homegateway-" << GIT_HASH << '.' << BUILD_TIME;

    SMSSender::instance().sendSMS(to, content.str());
}

void DeviceManager::sendDeviceList(const std::string &to)
{
    std::stringstream content;

    {
        std::lock_guard<std::mutex> guard(m_devices_mutex);
        unsigned int device_count = 0;
        std::list<std::string> device_names;

        for (auto &e : m_devices) {
            auto &d = e.second;
            if (d->isRegistered()) {
                device_count++;
                device_names.push_back(d->getName());
            }
        }

        if (device_count) {
            content << device_count << " devices registered\n";
            for (auto &n : device_names)
                content << n << '\n';
        } else {
            content << "No device registered\n";
        }
    }

    SMSSender::instance().sendSMS(to, content.str());
}
