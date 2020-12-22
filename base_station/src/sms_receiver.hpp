#ifndef SMS_RECEIVER_HPP
#define SMS_RECEIVER_HPP

#include <functional>
#include <string>
#include <thread>

typedef std::function<void(const std::string&, const std::string&)> SMSReceiverCallback;

class SMSReceiver {
public:

    explicit SMSReceiver(SMSReceiverCallback cb);
    ~SMSReceiver();

    void start();
    void stop();

private:
    void run();
    void parseSMS(const std::string &path);

    SMSReceiverCallback m_callback;
    std::thread *m_thread;
    bool m_running;
};

#endif
