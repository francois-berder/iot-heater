#include <functional>
#include <sstream>
#include "home_gateway.hpp"
#include "logger.hpp"
#include "device_server.hpp"
#include "sms_server.hpp"

int main(int argc, char **argv)
{
    (void)argc;
    Logger::instance().startLogging(".");
    {
        std::stringstream ss;
        ss << argv[0] << " started";
        Logger::info(ss.str());
    }

    HomeGateway device_manager;
    DeviceServer device_server(32322,
                               std::bind(&HomeGateway::handleNewDevice, &device_manager, std::placeholders::_1));
    device_server.start();

    SMSServer sms_server(std::bind(&HomeGateway::handleSMSCommand, &device_manager, std::placeholders::_1, std::placeholders::_2));
    sms_server.start();
    while (true) {
        device_manager.process();
    }

    sms_server.stop();
    device_server.stop();
    Logger::instance().stopLogging();

    return 0;
}
