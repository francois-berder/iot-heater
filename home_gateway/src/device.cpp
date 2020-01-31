#include "device.hpp"

void Device::setID(const std::string& id)
{
    m_id = id;
}

void Device::setName(const std::string &name)
{
    m_name = name;
}

std::string Device::getID() const
{
    return m_id;
}

std::string Device::getName() const
{
    return m_name;
}

void Device::addFeature(std::shared_ptr<DeviceFeature> f)
{
    m_features.push_back(f);
}
