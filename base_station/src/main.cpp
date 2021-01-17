#include <functional>
#include <iostream>
#include <sstream>
#include "base_station.hpp"
#include "logger.hpp"
#include "device_server.hpp"
#include "sms_receiver.hpp"
#include "version.hpp"
#include "web_server.hpp"

#define DEFAULT_DEVICE_SERVER_PORT  (32322)

static void print_help(char *program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "    --device-server-port <port>       Set device server port\n"
              << "    --version, -v                     Print version\n"
              << "    --help, -h                        Print help\n"
              << std::flush;
}

static void wait_ms(unsigned int ms)
{
    struct timespec req, rem;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms - req.tv_sec * 1000) * 1000 * 1000;
    while (nanosleep(&req, &rem))
        req = rem;
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
        } else if (opt == "--version" || opt == "-v") {
            std::cout << get_version_str() << std::endl;
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
        ss << program_name << " (version: " << get_version_str() <<  ") started";
        Logger::info(ss.str());
    }

    BaseStation base_station;
    DeviceServer device_server(device_server_port,
                               std::bind(&BaseStation::handleNewDevice, &base_station, std::placeholders::_1));
    device_server.start();

    SMSReceiver sms_receiver(std::bind(&BaseStation::handleSMSCommand, &base_station, std::placeholders::_1, std::placeholders::_2));
    sms_receiver.start();

    WebServer web_server(&base_station);
    web_server.start();

    while (true) {
        base_station.process();
        wait_ms(50);
    }

    web_server.stop();
    sms_receiver.stop();
    device_server.stop();
    Logger::instance().stopLogging();

    return 0;
}
