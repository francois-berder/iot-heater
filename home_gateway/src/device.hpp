#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "device_feature.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

typedef std::array<uint8_t, 6> DeviceUID;

class Device {
public:
    Device() = default;
    ~Device() = default;

    void setUID(const DeviceUID& uid);
    void setName(const std::string &name);
    void addFeature(std::shared_ptr<DeviceFeature> f);

    DeviceUID getUID() const;
    std::string getName() const;

    bool isRegistered() const;

    std::string serialize() const;

private:
    DeviceUID m_uid;
    std::string m_name;
    std::vector<std::shared_ptr<DeviceFeature>> m_features;
};

#endif
