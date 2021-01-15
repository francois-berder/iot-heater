#ifndef BASE_STATION_HPP
#define BASE_STATION_HPP

#include "timer.hpp"
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <time.h>

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
    std::string buildWebpage();

private:
    void handleConnections();

    void parseMessage(DeviceConnection &conn, uint8_t *data);
    void parseCommands();
    void sendVersion(const std::string &to);
    void checkStaleConnections();
    void sendHeaterState(int fd, const std::string &name);
    void checkWifi();
    void checkLostDevices();

    bool loadState();
    void saveState();

    std::list<DeviceConnection> m_connections;
    std::mutex m_connections_mutex;

    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;

    Timer m_stale_timer;
    HeaterState m_heater_default_state;
    std::map<std::string, HeaterState> m_heater_state;

    bool m_locked;
    std::set<std::string> m_phone_whitelist;

    std::string m_emergency_phone;
    Timer m_check_wifi_timer;
    unsigned int m_wifi_not_good_counter;

    uint64_t m_message_counter;
    std::map<uint64_t,uint64_t> m_heater_counter; /* MAC addr -> counter */
    std::map<uint64_t, time_t> m_heater_last_seen; /* MAC addr -> timestamp */
    Timer m_lost_devices_timer;
    std::map<uint64_t, std::string> m_heater_name; /* MAC addr -> name */
};

#endif
