#include "timer.h"

void TimerManager::add(int fd, std::time_t now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_last_active[fd] = now;
}

void TimerManager::touch(int fd, std::time_t now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::unordered_map<int, std::time_t>::iterator it = m_last_active.find(fd);
    if (it != m_last_active.end()) {
        it->second = now;
    }
}

void TimerManager::remove(int fd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_last_active.erase(fd);
}

std::vector<int> TimerManager::collect_expired(std::time_t now, int timeout_sec)
{
    std::vector<int> expired;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (std::unordered_map<int, std::time_t>::iterator it = m_last_active.begin(); it != m_last_active.end();) {
        if (now - it->second >= timeout_sec) {
            expired.push_back(it->first);
            it = m_last_active.erase(it);
        } else {
            ++it;
        }
    }

    return expired;
}
