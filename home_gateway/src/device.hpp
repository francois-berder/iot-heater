#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "device_feature.hpp"
#include <memory>
#include <string>
#include <vector>

class Device {
public:
    Device() = default;
    ~Device() = default;

    void setID(const std::string& id);
    void setName(const std::string &name);
    void addFeature(std::shared_ptr<DeviceFeature> f);

    std::string getID() const;
    std::string getName() const;

private:
    std::string m_id;
    std::string m_name;
    std::vector<std::shared_ptr<DeviceFeature>> m_features;
};

#endif
