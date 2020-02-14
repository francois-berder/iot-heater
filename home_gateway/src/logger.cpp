#include "logger.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unistd.h>

namespace {
    const int MAX_LOG_FILESIZE = 1024 * 1024;
    const unsigned int MAX_LOG_COUNT = 16;
}

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

void Logger::debug(const std::string &s)
{
    Logger::instance().log("DEBUG", s);
}

void Logger::startLogging(const std::string &dir)
{
    m_dir = dir;
    m_index = 0;

    std::stringstream ss;
    ss << m_dir << "/log-" << m_index << ".txt";
    m_file.open(ss.str(), std::fstream::out | std::fstream::trunc);
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

    if (m_file.tellp() >= MAX_LOG_FILESIZE) {
        m_file.close();
        m_index++;

        /* Do not keep too much logs */
        if (m_index >= MAX_LOG_COUNT) {
            std::stringstream ss;
            ss << m_dir << "/log-" << m_index - MAX_LOG_COUNT << ".txt";
            unlink(ss.str().c_str());
        }

        std::stringstream ss;
        ss << m_dir << "/log-" << m_index << ".txt";
        m_file.open(ss.str(), std::fstream::out | std::fstream::trunc);
    }
}
