#ifndef BASE_STATION_HPP
#define BASE_STATION_HPP

#include "heater.hpp"
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
    void sendHeaterState(int fd, HeaterState state);
    void checkWifi();
    void checkLostDevices();

    bool loadState();
    void saveState();

    std::list<DeviceConnection> m_connections;
    std::mutex m_connections_mutex;

    std::queue<std::pair<std::string,std::string>> m_commands;
    std::mutex m_commands_mutex;

    Timer m_stale_timer;
    
    /* State provided by the user */
    HeaterState m_heater_default_state;
    std::map<std::string, HeaterState> m_heater_state;

    bool m_locked;
    std::set<std::string> m_phone_whitelist;

    std::string m_emergency_phone;
    Timer m_check_wifi_timer;
    unsigned int m_wifi_not_good_counter;

    uint64_t m_message_counter;
    std::map<uint64_t,uint64_t> m_heater_counter; /* MAC addr -> counter */
    std::map<uint64_t, Heater> m_heaters;   /* MAC -> Heater */
    std::mutex m_heaters_mutex;
    Timer m_lost_devices_timer;
};

#endif
