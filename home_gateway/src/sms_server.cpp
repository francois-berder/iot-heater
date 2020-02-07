#include "logger.hpp"
#include "sms_server.hpp"
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SMSTOOL_INCOMING_DIR    "/var/spool/sms/incoming/"
#define BUF_LEN                 (sizeof(struct inotify_event) + NAME_MAX + 1)

SMSServer::SMSServer(SMSServerTextReceivedCallback cb):
m_callback(cb),
m_thread(nullptr),
m_running(false)
{

}

SMSServer::~SMSServer()
{
    if (m_running) {
        m_running = false;
        m_thread->join();
        delete m_thread;
    }
}

void SMSServer::start()
{
    if (m_running) {
        Logger::warn("Attempted to start already running sms server");
        return;
    }

    m_running = true;
    m_thread = new std::thread(&SMSServer::run, this);
}

void SMSServer::stop()
{
    if (!m_running) {
        Logger::warn("Attempted to stop already stopped sms server");
        return;
    }

    m_running = false;
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;
}

void SMSServer::run()
{
    struct pollfd fds[1];
    int wd;

    fds[0].fd = inotify_init();
    if (fds[0].fd  < 0) {
        Logger::err("inotify_init failed");
        throw std::runtime_error("inotify_init failed");
    }
    wd = inotify_add_watch(fds[0].fd, SMSTOOL_INCOMING_DIR, IN_MODIFY);
    if (wd < 0) {
        close(fds[0].fd);
        Logger::err("inotify_add_watch failed");
        throw std::runtime_error("inotify_add_watch failed");
    }
    fds[0].events = POLLIN;

    while (m_running) {
        struct inotify_event *event;
        char __attribute__ ((aligned(8))) buf[BUF_LEN] ;
        int ret;

        ret = poll(fds, 1, 100);
        if (ret < 0) {
            std::stringstream ss;
            ss << "Failed to poll in sms server, error " << ret;
            Logger::err(ss.str());
            throw std::runtime_error(ss.str());
        } else if (ret == 0) {
            continue;
        }

        ret = read(fds[0].fd, buf, BUF_LEN);
        if (ret < 0) {
            Logger::err("sms_server read failed\n");
            continue;
        }

        event = (struct inotify_event *) buf;
        std::stringstream ss;
        ss << SMSTOOL_INCOMING_DIR << event->name;
        parseSMS(ss.str());
    }

    inotify_rm_watch(fds[0].fd, wd);
    close(fds[0].fd);
}

void SMSServer::parseSMS(const std::string &path)
{
    std::ifstream file(path);

    if (!file) {
        std::stringstream ss;
        ss << "Failed to read content of SMS \"" << path << "\"";
        Logger::err(ss.str());
        return;
    }

    std::stringstream content;
    content << file.rdbuf();

    /*
     * SMS text message layout is as follows:
     *
     * From:
     * <empty line>
     * SMS text
     */
    std::string line;
    std::string from;
    if (!std::getline(content, from, '\n')) {
        return;
    }

    if (from.rfind("From: ")) {
        return;
    }

    from.erase(0, 6);

    /* Skip header */
    while (std::getline(content, line, '\n')) {
        if (line.empty())
            break;
    }

    {
        std::stringstream ss;
        ss << "Parsed SMS file \"" << path << "\"";
        Logger::info(ss.str());
    }

    std::string text;
    while (std::getline(content, line, '\n'))
        text += line;

    m_callback(from, text);
}
