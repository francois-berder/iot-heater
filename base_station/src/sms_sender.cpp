#include "logger.hpp"
#include "sms_sender.hpp"
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#define MODULE_3G_DEVPATH "/dev/ttyUSB2"
#define SMS_OUTGOING_DIR "/var/spool/sms/outgoing/"
#define SMS_TOO_OLD         (15 * 60)   /* in seconds */

SMSSender::SMSSender():
m_mutex(),
m_counter(0)
{
    cleanOutgoingDir();
}

SMSSender& SMSSender::instance()
{
    static SMSSender s;
    return s;
}

void SMSSender::cleanOutgoingDir() {
    DIR *outgoing_dir;
    struct dirent *next_file;
    outgoing_dir = opendir(SMS_OUTGOING_DIR);
    if (outgoing_dir == NULL) {
        Logger::err("Failed to open directory " SMS_OUTGOING_DIR);
        return;
    }

    while ((next_file = readdir(outgoing_dir)) != NULL)
    {
        if (next_file->d_type != DT_REG)
            continue;

        {
            std::stringstream ss;
            ss << "Removing old SMS " << next_file->d_name;
            Logger::info(ss.str());
        }
        std::string filepath = SMS_OUTGOING_DIR;
        filepath += next_file->d_name;
        if (remove(filepath.c_str()) != 0) {
            std::stringstream ss;
            ss << "Failed to delete old SMS ";
            ss << next_file->d_name;
            Logger::err(ss.str());
        }
    }
    closedir(outgoing_dir);
}

bool SMSSender::sendSMS(const std::string &to, const std::string &content)
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
        ss << MODULE_3G_DEVPATH << " found). Discarding text message.";
        Logger::err(ss.str());
        return false;
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

    /* Move it to smstool outgoing dir */
    std::stringstream path;
    path << SMS_OUTGOING_DIR << filename.str();
    if (rename(tmp_path.str().c_str(), path.str().c_str()) < 0) {
        Logger::err("Failed to send SMS\n");
        return false;
    }

    /* Keep track of this file to delete it later */
    m_old_files[filename.str()] = std::chrono::steady_clock::now();

    return true;
}

void SMSSender::cleanupSMS()
{
    auto timestamp_now = std::chrono::steady_clock::now();

    for (auto &e : m_old_files) {
        std::string filename = e.first;
        auto timestamp = e.second;
        auto sms_age = std::chrono::duration_cast<std::chrono::seconds>(timestamp_now - timestamp).count();

        if (sms_age < SMS_TOO_OLD)
            continue;

        {
            std::stringstream ss;
            ss << "Deleting old SMS file ";
            ss << filename;
            Logger::info(ss.str());
        }
        std::string filepath = SMS_OUTGOING_DIR;
        filepath += filename;
        if (remove(filepath.c_str()) != 0) {
            std::stringstream ss;
            ss << "Failed to delete file ";
            ss << filepath;
            Logger::err(ss.str());
        }
    }
    m_old_files.clear();
}
