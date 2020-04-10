#ifndef BASE_STATION_HPP
#define BASE_STATION_HPP

#include "timer.hpp"
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

struct DeviceConnection {
    int fd;
    std::chrono::steady_clock::time_point last_seen;
};

enum HeaterState {
    HEATER_OFF,
    HEATER_DEFROST,
    HEATER_ECO,
    HEATER_COMFORT,
};

class BaseStation {
public:
    BaseStation();
    ~BaseStation();

    void process();

    void handleNewDevice(int fd);
    void handleSMSCommand(const std::string &from, const std::string &content);

private:
    void handleConnections();
    void handleTimers();

    void parseMessage(DeviceConnection &conn, uint8_t *data);
    void parseCommands();
    void sendVersion(const std::string &to);
    void checkStaleConnections();
    void broadcastHeaterState();
    void sendHeaterState(int fd);

    bool loadState();
    void saveState();

    std::list<DeviceConnection> m_connections;
    std::mutex m_connections_mutex;

    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;

    Timer m_stale_timer;
    HeaterState m_heater_state;
};

#endif
