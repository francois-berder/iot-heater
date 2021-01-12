#include "logger.hpp"
#include "sms_sender.hpp"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#define MODULE_3G_DEVPATH "/dev/ttyUSB2"
#define SMS_OUTGOING_DIR "/var/spool/sms/outgoing/"

SMSSender::SMSSender():
m_mutex(),
m_counter(0)
{

}

SMSSender& SMSSender::instance()
{
    static SMSSender s;
    return s;
}

void SMSSender::sendSMS(const std::string &to, const std::string &content)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    /* Check if 3G module is connected.
     * We do not want to write the message to a file
     * otherwise if the user connects the 3G module
     * again, the emergency phone might get overflowed with
     * tons of messages.
     */
    if (access(MODULE_3G_DEVPATH, F_OK ) != 0) {
        std::stringstream ss;
        ss << "3G module not detected (no ";
        ss << MODULE_3G_DEVPATH << "found). Discarding text message.";
        Logger::err(ss.str());
        return;
    }

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
