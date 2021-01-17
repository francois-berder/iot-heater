#ifndef HEATER_HPP
#define HEATER_HPP

#include <string>
#include <time.h>

enum HeaterState {
    HEATER_OFF,
    HEATER_DEFROST,
    HEATER_ECO,
    HEATER_COMFORT,
};

class Heater {
public:
    Heater(const std::string &name = std::string());

    void update(HeaterState newState);

    std::string getName() const;
    time_t getLastRequestTimestamp() const;
    HeaterState getState() const;

private:
    std::string m_name;
    time_t m_last_request_timestamp;
    HeaterState m_state;
};

#endif
