#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include <microhttpd.h>
#include "base_station.hpp"

class WebServer {
public:
    explicit WebServer(BaseStation *b);
    ~WebServer();

    void start();
    void stop();

private:

    BaseStation *m_base_station;
    struct MHD_Daemon *m_daemon;
};

#endif
