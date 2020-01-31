#ifndef DEVICE_FEATURE_HPP
#define DEVICE_FEATURE_HPP

#include <string>

class DeviceFeature {
public:
    DeviceFeature(const std::string &name);
    virtual ~DeviceFeature() = default;

    std::string getName() const;

private:
    std::string m_name;
};

#endif
