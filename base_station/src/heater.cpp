#include "heater.hpp"

Heater::Heater(const std::string &name, const std::string &ip_addr):
m_name(name),
m_ip_addr(ip_addr),
m_last_request_timestamp(0),
m_state(HEATER_DEFROST)
{

}

void Heater::update(HeaterState newState)
{
    m_last_request_timestamp = time(NULL);
    m_state = newState;
}

std::string Heater::getName() const
{
    return m_name;
}

std::string Heater::getLastIPAddress() const
{
    return m_ip_addr;
}

time_t Heater::getLastRequestTimestamp() const
{
    return m_last_request_timestamp;
}

HeaterState Heater::getState() const
{
    return m_state;
}
