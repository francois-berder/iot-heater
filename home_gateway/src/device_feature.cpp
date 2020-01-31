#include "device_feature.hpp"
#include <string>

DeviceFeature::DeviceFeature(const std::string &name):
m_name(name)
{
}

std::string DeviceFeature::getName() const
{
    return m_name;
}
