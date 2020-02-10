#ifndef DEVICE_MANAGER_HPP
#define DEVICE_MANAGER_HPP

#include "device.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

class DeviceManager {
public:
    DeviceManager() = default;
    ~DeviceManager();

    void process();

    void handleNewDevice(int fd);
    void handleSMSCommand(const std::string &from, const std::string &content);

private:
    void parseMessage(int fd, uint8_t type, uint8_t *data, int len);
    void parseCommands();
    void saveToFile();

    std::map<unsigned int, std::shared_ptr<Device>> m_devices;
    std::mutex m_devices_mutex;
    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;
};

#endif
