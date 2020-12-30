#ifndef SMS_SENDER_HPP
#define SMS_SENDER_HPP

#include <cstdint>
#include <mutex>
#include <string>


class SMSSender {
public:
    SMSSender(const SMSSender &l) = delete;
    SMSSender(SMSSender&&) = delete;
    SMSSender& operator=(const SMSSender &l) = delete;
    SMSSender& operator=(SMSSender&&) = delete;

    static SMSSender& instance();

    void sendSMS(const std::string& to, const std::string &content);

private:

    SMSSender();
    ~SMSSender() = default;

    std::mutex m_mutex;
    uint64_t m_counter;
};

#endif
