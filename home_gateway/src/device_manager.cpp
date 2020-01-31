#include "device_manager.hpp"
#include "heater_feature.hpp"
#include "logger.hpp"
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <unistd.h>

enum MessageType {
    DEVICE_REGISTER,
};

enum DeviceFeatureID {
    HEATER,
};

DeviceManager::~DeviceManager()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    /* Close all file descriptors */
    for (auto& d : m_devices)
        close(d.first);
}

void DeviceManager::process()
{
    struct pollfd *fds;
    size_t nfds;

    {
        std::lock_guard<std::mutex> guard(m_mutex);

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
    } else if (ret == 0) {
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
    std::lock_guard<std::mutex> guard(m_mutex);

    m_devices[fd] = std::shared_ptr<Device>(new Device);
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
        } else {
            std::stringstream ss;
            ss << "Ignored invalid feature " << data[52];
            Logger::warn(ss.str());
        }
    }
}
