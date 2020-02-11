#include <functional>
#include <sstream>
#include "home_gateway.hpp"
#include "logger.hpp"
#include "device_server.hpp"
#include "sms_server.hpp"

#define DEFAULT_DEVICE_SERVER_PORT     (32322)

static void print_help(char *program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options: \n"
              << "    --device-server-port <port>       Set device server port\n"
              << "    --help, -h                        Print help\n"
              << std::flush;
}

int main(int argc, char **argv)
{
    char *program_name = argv[0];
    int device_server_port = DEFAULT_DEVICE_SERVER_PORT;

    argc--;
    argv++;
    while (argc) {
        std::string opt(argv[0]);
        if (opt == "--device-server-port" && argc >= 2) {
            std::string optarg(argv[1]);
            device_server_port = std::stoi(optarg);
            argc--;
            argv++;
        } else if (opt == "--help" || opt == "-h") {
            print_help(program_name);
            return 0;
        } else {
            std::cerr << "Invalid option: \"" << opt << '\"' << std::endl;
            print_help(program_name);
            return -1;
        }

        argc--;
        argv++;
    }


    Logger::instance().startLogging(".");
    {
        std::stringstream ss;
        ss << program_name << " started";
        Logger::info(ss.str());
    }

    HomeGateway device_manager;
    DeviceServer device_server(device_server_port,
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
