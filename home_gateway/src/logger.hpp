#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

class Logger {
public:
    Logger(const Logger &l) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger &l) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger& instance();

    static void err(const std::string &s);
    static void warn(const std::string &s);
    static void info(const std::string &s);

    void startLogging(const std::string &dir);
    void stopLogging();

private:
    Logger() = default;
    ~Logger() = default;
    void log(const std::string &prefix, const std::string &s);

    std::string m_dir;
    unsigned long long m_index;
    std::ofstream m_file;
    std::mutex m_mutex;
};

#endif
