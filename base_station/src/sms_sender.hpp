#ifndef SMS_SENDER_HPP
#define SMS_SENDER_HPP

#include <chrono>
#include <cstdint>
#include <map>
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

    /**
     * @brief Clean old SMS to avoid having too many files
     * in the SMS outgoing directory.
     *
     * This function should periodically be called.
     */
    void cleanupSMS();

private:

    SMSSender();
    ~SMSSender() = default;

    void cleanOutgoingDir();

    std::mutex m_mutex;
    uint64_t m_counter;
    std::map<std::string, std::chrono::time_point<std::chrono::steady_clock>> m_old_files;
};

#endif
