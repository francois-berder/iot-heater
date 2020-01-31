#include "heater_feature.hpp"

HeaterFeature::HeaterFeature():
DeviceFeature("heater"),
m_mode(HeaterMode::OFF)
{
}

void HeaterFeature::setMode(enum HeaterMode mode)
{
    m_mode = mode;
}
