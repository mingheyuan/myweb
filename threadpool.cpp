#include "threadpool.h"

ThreadPool::ThreadPool(std::size_t thread_count) : m_stop(false)
{
    if (thread_count == 0) {
        thread_count = 1;
    }

    for (std::size_t i = 0; i < thread_count; ++i) {
        m_workers.push_back(std::thread(&ThreadPool::worker_loop, this));
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();

    for (std::size_t i = 0; i < m_workers.size(); ++i) {
        if (m_workers[i].joinable()) {
            m_workers[i].join();
        }
    }
}

void ThreadPool::enqueue(const std::function<void()> &task)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stop) {
            return;
        }
        m_tasks.push(task);
    }
    m_cv.notify_one();
}

void ThreadPool::worker_loop()
{
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });

            if (m_stop && m_tasks.empty()) {
                return;
            }

            task = m_tasks.front();
            m_tasks.pop();
        }

        task();
    }
}
