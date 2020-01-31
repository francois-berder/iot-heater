#ifndef DEVICE_SERVER_HPP
#define DEVICE_SERVER_HPP

#include <functional>
#include <thread>

typedef std::function<void(int)> DeviceServerNewDeviceCallback;

class DeviceServer {
public:

    DeviceServer(unsigned int serverPort, DeviceServerNewDeviceCallback cb);
    ~DeviceServer();

    void start();
    void stop();

private:
    void run();

    bool m_running;
    std::thread *m_thread;
    int m_fd;
    unsigned int m_serverPort;
    DeviceServerNewDeviceCallback m_callback;
};

#endif
