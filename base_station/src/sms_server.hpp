#ifndef SMS_SERVER_HPP
#define SMS_SERVER_HPP

#include <functional>
#include <string>
#include <thread>

typedef std::function<void(const std::string&, const std::string&)> SMSServerTextReceivedCallback;

class SMSServer {
public:

    SMSServer(SMSServerTextReceivedCallback cb);
    ~SMSServer();

    void start();
    void stop();
    void send_sms(const std::string &text);

private:
    void run();
    void parseSMS(const std::string &path);

    SMSServerTextReceivedCallback m_callback;
    std::thread *m_thread;
    bool m_running;
};

#endif
