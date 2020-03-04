#include "device.hpp"
#include <iomanip>
#include <sstream>

std::string uid_to_string(DeviceUID uid)
{
    std::stringstream ss;
    ss << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[0]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[1]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[2]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[3]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[4]);
    ss << ':' << std::uppercase << std::setfill('0') << std::setw(2) <<  std::hex << std::to_string(uid[5]);
    return ss.str();
}
