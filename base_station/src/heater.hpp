#ifndef HEATER_HPP
#define HEATER_HPP

#include <ctime>
#include <string>

enum HeaterState {
    HEATER_OFF,
    HEATER_DEFROST,
    HEATER_ECO,
    HEATER_COMFORT,
};

class Heater {
public:
    explicit Heater(const std::string &name = std::string(), const std::string &ip_addr = std::string());

    void update(HeaterState newState);

    std::string getName() const;
    std::string getLastIPAddress() const;
    time_t getLastRequestTimestamp() const;
    HeaterState getState() const;

private:
    std::string m_name;
    std::string m_ip_addr;
    time_t m_last_request_timestamp;
    HeaterState m_state;
};

#endif
