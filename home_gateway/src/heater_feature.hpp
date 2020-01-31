#ifndef HEATER_FEATURE_HPP
#define HEATER_FEATURE_HPP

#include "device_feature.hpp"

enum HeaterMode {
    OFF,
    DEFROST,
    COMFY,
    ECO,
};

class HeaterFeature : public DeviceFeature {
public:
    HeaterFeature();
    virtual ~HeaterFeature() = default;

    void setMode(enum HeaterMode mode);

private:
    enum HeaterMode m_mode;
};

#endif
