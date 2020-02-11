#ifndef HOME_GATEWAY_HPP
#define HOME_GATEWAY_HPP

#include "device.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

class HomeGateway {
public:
    HomeGateway() = default;
    ~HomeGateway();

    void process();

    void handleNewDevice(int fd);
    void handleSMSCommand(const std::string &from, const std::string &content);

private:
    void parseMessage(int fd, uint8_t type, uint8_t *data, int len);
    void parseCommands();
    void saveToFile();
    void sendVersion(const std::string &to);
    void sendDeviceList(const std::string &to);

    std::map<unsigned int, std::shared_ptr<Device>> m_devices;
    std::mutex m_devices_mutex;
    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;
};

#endif
