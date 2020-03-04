#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

struct DeviceConnection {
    int fd;
    std::chrono::steady_clock::time_point last_seen;
};

typedef std::array<uint8_t, 6> DeviceUID;
struct Device {
    DeviceConnection conn;
    DeviceUID uid;
    std::string name;
};

std::string uid_to_string(DeviceUID uid);

#endif
