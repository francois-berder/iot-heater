#include "device_server.hpp"
#include "logger.hpp"
#include <arpa/inet.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>


DeviceServer::DeviceServer(unsigned int serverPort, DeviceServerNewDeviceCallback cb):
m_running(false),
m_thread(nullptr),
m_fd(-1),
m_serverPort(serverPort),
m_callback(cb)
{

}

DeviceServer::~DeviceServer()
{
    if (m_running) {
        m_thread->join();
        delete m_thread;
    }
    close(m_fd);
}

void DeviceServer::start()
{
    struct sockaddr_in server_addr;

    if (m_running) {
        Logger::warn("Attempting to start already running device server");
        return;
    }

    /* Start listening on port */
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        Logger::err("Failed to create socket for device server");
        throw std::runtime_error("Failed to create socket for device server");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(m_serverPort);

    if (bind(m_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(m_fd);
        m_fd = -1;
        Logger::err("Failed to bind device server socket");
    }

    if (listen(m_fd, 10) < 0) {
        close(m_fd);
        m_fd = -1;
        std::stringstream ss;
        ss << "Failed to listen on port " << m_serverPort;
        Logger::err(ss.str());
        throw std::runtime_error(ss.str());
    }

    /* Start thread */
    m_running = true;
    m_thread = new std::thread(&DeviceServer::run, this);
}

void DeviceServer::stop()
{
    if (!m_running)
        return;

    m_running = false;
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;

    Logger::info("Device server stopped");
}

void DeviceServer::run()
{
    {
        std::stringstream ss;
        ss << "Device server started. Listening on port " << m_serverPort;
        Logger::info(ss.str());
    }

    struct pollfd fds[1];
    fds[0].fd = m_fd;
    fds[0].events = POLLIN;
    while (m_running) {
        int ret = poll(fds, 1, 100);
        if (ret < 0) {
            std::stringstream ss;
            ss << "Device server poll error " << ret;
            Logger::err(ss.str());
            throw std::runtime_error(ss.str());
        } else if (ret == 0) {
            continue;
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);

            int client_fd = accept(m_fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
            if (client_fd < 0)
                continue;

            std::stringstream ss;
            ss << "New device connection from " << inet_ntoa(addr.sin_addr);
            Logger::debug(ss.str());

            if (m_callback)
                m_callback(client_fd);
            else
                close(client_fd);
        }
    }
}
