#include "version.hpp"
#include <string>
#include <sstream>

#ifndef GIT_HASH
#define GIT_HASH        "development"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME      "unknown-time"
#endif

std::string get_version_str()
{
    std::stringstream ss;
    ss << "base_station-" << GIT_HASH << '.' << BUILD_TIME;
    return ss.str();
}
