#include <cstring>
#include <microhttpd.h>
#include <stdexcept>
#include <sstream>
#include "logger.hpp"
#include "web_server.hpp"

#define DEFAULT_WEBSERVER_PORT  (80)

namespace {

int answerConnection(void *cls, struct MHD_Connection *connection,
                                const char *url, const char *method,
                                const char *version, const char *upload_data,
                                size_t *upload_data_size, void **con_cls)
{
    struct MHD_Response *response;
    int ret;
    BaseStation *b = reinterpret_cast<BaseStation*>(cls);
    (void) url;               /* Unused. Silent compiler warning. */
    (void) method;            /* Unused. Silent compiler warning. */
    (void) version;           /* Unused. Silent compiler warning. */
    (void) upload_data;       /* Unused. Silent compiler warning. */
    (void) upload_data_size;  /* Unused. Silent compiler warning. */
    (void) con_cls;           /* Unused. Silent compiler warning. */

    std::string page = b->buildWebpage();

    char *buf = (char *)malloc(page.length() + 1);
    strcpy(buf, page.c_str());
    response = MHD_create_response_from_buffer(page.length(), buf, MHD_RESPMEM_MUST_FREE);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

}

WebServer::WebServer(BaseStation *b):
m_base_station(b),
m_daemon(nullptr)
{

}

WebServer::~WebServer()
{
    stop();
}

void WebServer::start()
{
    if (m_daemon) {
        Logger::warn("Attempted to start already running web server");
        return;
    }

    m_daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                               DEFAULT_WEBSERVER_PORT, NULL, NULL,
                               answerConnection, m_base_station, MHD_OPTION_END);

    if (!m_daemon) {
        Logger::err("Failed to start web server");
        throw std::runtime_error("Failed to start web server");
    } else {
        std::stringstream ss;
        ss << "Web server started. Listening on port " << DEFAULT_WEBSERVER_PORT;
        Logger::info(ss.str());
    }
}

void WebServer::stop()
{
    if (m_daemon) {
        MHD_stop_daemon(m_daemon);
    } else {
        Logger::warn("Attempted to stop already stopped web server");
        return;
    }
}
