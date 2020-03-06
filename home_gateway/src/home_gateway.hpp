#ifndef HOME_GATEWAY_HPP
#define HOME_GATEWAY_HPP

#include "device.hpp"
#include "timer.hpp"
#include <cstdint>
#include <list>
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
    void handleConnections();
    void handleDevices();
    void handleTimers();

    void parseMessage(int fd, uint8_t *data);
    void parseCommands();
    void sendVersion(const std::string &to);
    void sendDeviceList(const std::string &to);
    bool lookup_device(DeviceUID uid);
    void checkStaleConnectionsAndDevices();

    /* Connections not yet associated with a UID */
    std::list<DeviceConnection> m_connections;
    std::mutex m_connections_mutex;

    std::list<Device> m_devices;

    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;

    Timer m_stale_timer;
};

#endif
