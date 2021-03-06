#include "logger.hpp"
#include "sms_sender.hpp"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#define SMS_OUTGOING_DIR "/var/spool/sms/outgoing/"

SMSSender::SMSSender():
m_mutex(),
m_verboseLevel(SMS_SENDER_VERBOSE),
m_counter(0)
{

}

SMSSender& SMSSender::instance()
{
    static SMSSender s;
    return s;
}

void SMSSender::sendErrorSMS(const std::string& to, const std::string &content)
{
    sendSMS(to, content);
}

void SMSSender::sendWarningSMS(const std::string& to, const std::string &content)
{
    sendSMS(to, content);
}

void SMSSender::sendVerboseSMS(const std::string& to, const std::string &content)
{
    if (m_verboseLevel > SMS_SENDER_VERBOSE)
        return;

    sendSMS(to, content);
}

void SMSSender::sendDebugSMS(const std::string& to, const std::string &content)
{
    if (m_verboseLevel > SMS_SENDER_DEBUG)
        return;
    sendSMS(to, content);
}

void SMSSender::sendSMS(const std::string &to, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    std::ofstream file;

    std::stringstream filename;
    filename << "sms_" << getpid() << "_"  << m_counter;

    m_counter++;

    std::stringstream tmp_path;
    tmp_path << "/tmp/" << filename.str();
    file.open(tmp_path.str());

    /* Create temporary file */
    file << "To: " << to << '\n';
    file << '\n';
    file << content << '\n';
    file.close();

    /* Move it smstool outgoind dir */
    std::stringstream path;
    path << SMS_OUTGOING_DIR << filename.str();
    if (rename(tmp_path.str().c_str(), path.str().c_str()) < 0)
        Logger::err("Failed to send SMS\n");
}

void SMSSender::setVerboseLevel(enum SMSServerVerboseLevel verboseLevel)
{
    if (m_verboseLevel == verboseLevel)
        return;

    std::stringstream ss;
    ss << "Setting SMS log level to ";
    if (verboseLevel == SMS_SENDER_VERBOSE)
        ss << "DEBUG";
    else if (verboseLevel == SMS_SENDER_VERBOSE)
        ss << "VERBOSE";
    else if (verboseLevel == SMS_SENDER_QUIET)
        ss << "QUIET";
    Logger::info(ss.str());

    m_verboseLevel = verboseLevel;
}
