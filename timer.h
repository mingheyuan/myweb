#ifndef TIMER_H
#define TIMER_H

#include <ctime>
#include <mutex>
#include <unordered_map>
#include <vector>

class TimerManager {
public:
    void add(int fd, std::time_t now);
    void touch(int fd, std::time_t now);
    void remove(int fd);
    std::vector<int> collect_expired(std::time_t now, int timeout_sec);

private:
    std::unordered_map<int, std::time_t> m_last_active;
    std::mutex m_mutex;
};

#endif
