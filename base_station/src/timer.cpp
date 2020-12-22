#include "timer.hpp"
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>

Timer::Timer():
fd(-1)
{
}

Timer::~Timer()
{
    if (fd >= 0)
        close(fd);
}

void Timer::start(unsigned int period_ms, bool periodic)
{
    if (fd < 0) {
        fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (fd < 0)
            throw std::runtime_error("Failed to create timer");
    }

    struct itimerspec val;
    val.it_value.tv_sec = period_ms / 1000;
    val.it_value.tv_nsec = (period_ms - val.it_value.tv_sec  * 1000) * 1000 * 1000;
    if (periodic) {
        val.it_interval.tv_nsec = val.it_value.tv_nsec;
        val.it_interval.tv_sec = val.it_value.tv_sec;
    }
    timerfd_settime(fd, 0, &val, NULL);
}

void Timer::stop()
{
    if (fd < 0)
        return;

    struct itimerspec val;
    val.it_interval.tv_nsec = 0;
    val.it_interval.tv_sec = 0;
    val.it_value.tv_nsec = 0;
    val.it_value.tv_sec = 0;
    timerfd_settime(fd, 0, &val, NULL);
}

int Timer::getFD() const
{
    return fd;
}
