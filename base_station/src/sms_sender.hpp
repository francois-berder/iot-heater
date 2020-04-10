#ifndef SMS_SENDER_HPP
#define SMS_SENDER_HPP

#include <cstdint>
#include <mutex>
#include <string>

enum SMSServerVerboseLevel {
    SMS_SENDER_DEBUG,
    SMS_SENDER_VERBOSE,
    SMS_SENDER_QUIET,
};

class SMSSender {
public:
    SMSSender(const SMSSender &l) = delete;
    SMSSender(SMSSender&&) = delete;
    SMSSender& operator=(const SMSSender &l) = delete;
    SMSSender& operator=(SMSSender&&) = delete;

    static SMSSender& instance();

    void sendErrorSMS(const std::string& to, const std::string &content);
    void sendWarningSMS(const std::string& to, const std::string &content);
    void sendVerboseSMS(const std::string& to, const std::string &content);
    void sendDebugSMS(const std::string& to, const std::string &content);
    void sendSMS(const std::string& to, const std::string &content);

    void setVerboseLevel(enum SMSServerVerboseLevel verboseLevel);

private:

    SMSSender();
    ~SMSSender() = default;

    std::mutex m_mutex;
    enum SMSServerVerboseLevel m_verboseLevel;
    uint64_t m_counter;
};

#endif
