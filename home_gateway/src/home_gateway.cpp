#include "heater_feature.hpp"
#include "home_gateway.hpp"
#include "logger.hpp"
#include "sms_sender.hpp"
#include <fstream>
#include <iomanip>
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
    PING,
    REGISTER,
    ACK,
    NAK
};

enum DeviceFeatureID {
    HEATER,
};

HomeGateway::~HomeGateway()
{
    std::lock_guard<std::mutex> guard(m_devices_mutex);

    /* Close all file descriptors */
    for (auto& d : m_devices)
        close(d.first);
}

void HomeGateway::process()
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
        } else if (ret > 0) {
            parseMessage(fds[i].fd, buffer, ret);
        }
    }
}

/*
 * Beware this function is called from device_server context !
 * That is why we need a mutex around the use of m_devices.
 */
void HomeGateway::handleNewDevice(int fd)
{
    std::lock_guard<std::mutex> guard(m_devices_mutex);

    m_devices[fd] = std::shared_ptr<Device>(new Device);
}

/*
 * Beware this function is called from sms_server context !
 */
void HomeGateway::handleSMSCommand(const std::string &from, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_commands_mutex);
    m_commands.emplace(from, content);
}

void HomeGateway::parseMessage(int fd, uint8_t *data, int len)
{
    DeviceUID uid;
    uint16_t type;

    if ((unsigned int)len < uid.size() + sizeof(type)) {
        std::stringstream ss;
        ss << "Message too short";
        Logger::warn(ss.str());
        return;
    }

    memcpy(uid.data(), data, uid.size());
    memcpy(&type, &data[6], sizeof(type));
    len -= sizeof(uid) + sizeof(type);
    data += sizeof(uid) + sizeof(type);

    if (type == MessageType::REGISTER) {
        /* name: 32 bytes, feature_count = 1, feature params */
        if (len < 33) {
            std::stringstream ss;
            ss << "Received malformed DEVICE_REGISTER len=" << len;
            Logger::warn(ss.str());
            return;
        }

        char name[33];
        memcpy(name, data, 32);
        name[32] = '\0';
        data += 32;
        len -= 32;

        /* Let's check if another device has this name */
        for (auto &d: m_devices) {
            auto dev = d.second;
            if (dev->getName() == std::string(name) && d.first != fd && uid != dev->getUID()) {
                std::stringstream ss;
                ss << "Device ";
                DeviceUID uid = dev->getUID();
                ss << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[0]);
                ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[1]);
                ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[2]);
                ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[3]);
                ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[4]);
                ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[5]);
                ss <<" already has the same name";
                Logger::warn(ss.str());
            }
        }

        m_devices[fd]->setUID(uid);
        m_devices[fd]->setName(name);

        {
            std::stringstream ss;
            ss << "Registering " << m_devices[fd]->serialize();
            std::string tmp = ss.str();
            Logger::info(ss.str());
        }

        uint8_t feature_count = data[0];
        data++;
        len--;

        while (feature_count) {
            if (len == 0) {
                Logger::err("Register message unexpectedly short");
                break;
            }

            uint8_t feature_id = *data++;
            feature_count--;
            len--;

            /* Parse feature list */
            /* For now, we assume we only deal with simple heater */
            if (feature_id == DeviceFeatureID::HEATER) {
                m_devices[fd]->addFeature(std::shared_ptr<DeviceFeature>(new HeaterFeature));
                saveToFile();
            } else {
                std::stringstream ss;
                ss << "Ignored invalid feature " << data[52];
                Logger::warn(ss.str());
            }
        }
    } else {
        std::stringstream ss;
        ss << "Received unknown message type " << type << " from ";
        Logger::warn(ss.str());
    }
}

void HomeGateway::parseCommands()
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

void HomeGateway::saveToFile()
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

void HomeGateway::sendVersion(const std::string &to)
{
    std::stringstream content;

    content << "homegateway-" << GIT_HASH << '.' << BUILD_TIME;

    SMSSender::instance().sendSMS(to, content.str());
}

void HomeGateway::sendDeviceList(const std::string &to)
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
