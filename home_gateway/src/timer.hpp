#ifndef TIMER_HPP
#define TIMER_HPP

class Timer {
public:
    Timer();
    virtual ~Timer();

    void start(unsigned int period_ms, bool periodic = false);
    void stop();

    int getFD();

private:
    int fd;
};

#endif
