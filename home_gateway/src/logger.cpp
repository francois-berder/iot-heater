#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include "logger.hpp"


Logger& Logger::instance()
{
    static Logger l;
    return l;
}

void Logger::err(const std::string &s)
{
    Logger::instance().log("ERR", s);
}

void Logger::warn(const std::string &s)
{
    Logger::instance().log("WARN", s);
}

void Logger::info(const std::string &s)
{
    Logger::instance().log("INFO", s);
}

void Logger::startLogging()
{
    m_file.open("log.txt", std::fstream::out | std::fstream::trunc);
}

void Logger::stopLogging()
{
    m_file.close();
}

void Logger::log(const std::string &prefix, const std::string &s)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    std::time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    char buffer[256];
    strftime(buffer, sizeof(buffer) - 1, "%F %T", localtime(&tt));
    buffer[255] = '\0';

    std::cout << '[' << buffer << "][" << prefix << "] " << s << std::endl;
    m_file << '[' << buffer << "][" << prefix << "] " << s << '\n';
    m_file.flush();
}
