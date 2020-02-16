#include "device.hpp"
#include <iomanip>
#include <sstream>


void Device::setUID(const DeviceUID& uid)
{
    m_uid = uid;
}

void Device::setName(const std::string &name)
{
    m_name = name;
}

DeviceUID Device::getUID() const
{
    return m_uid;
}

std::string Device::getName() const
{
    return m_name;
}

void Device::addFeature(std::shared_ptr<DeviceFeature> f)
{
    m_features.push_back(f);
}

bool Device::isRegistered() const
{
    return !m_name.empty();
}

std::string Device::serialize() const
{
    std::stringstream ss;

    ss << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[0]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[1]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[2]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[3]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[4]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(m_uid[5]);
    ss << ',' << m_name;
    return ss.str();
}
