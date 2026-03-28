#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger() : m_initialized(false), m_async_mode(false), m_stopping(false)
{
}

Logger::~Logger()
{
    shutdown();
}

bool Logger::init(const std::string &file_path, bool async_mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    m_file.open(file_path.c_str(), std::ios::out | std::ios::app);
    if (!m_file.is_open()) {
        return false;
    }

    m_async_mode = async_mode;
    m_stopping = false;
    m_initialized = true;

    if (m_async_mode) {
        m_worker = std::thread(&Logger::worker_loop, this);
    }

    return true;
}

void Logger::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) {
            return;
        }
        m_stopping = true;
    }

    m_cv.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) {
            m_file << m_queue.front() << std::endl;
            m_queue.pop();
        }
        m_file.flush();
        m_file.close();
        m_initialized = false;
        m_async_mode = false;
        m_stopping = false;
    }
}

void Logger::info(const std::string &msg)
{
    write_line("INFO", msg);
}

void Logger::warn(const std::string &msg)
{
    write_line("WARN", msg);
}

void Logger::error(const std::string &msg)
{
    write_line("ERROR", msg);
}

void Logger::write_line(const std::string &level, const std::string &msg)
{
    std::string line = build_line(level, msg);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        std::cerr << line << std::endl;
        return;
    }

    if (m_async_mode) {
        m_queue.push(line);
        m_cv.notify_one();
        return;
    }

    m_file << line << std::endl;
    m_file.flush();
}

std::string Logger::build_line(const std::string &level, const std::string &msg) const
{
    std::time_t t = std::time(nullptr);
    std::tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &t);
#else
    localtime_r(&t, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
        << " [" << level << "] "
        << msg;
    return oss.str();
}

void Logger::worker_loop()
{
    while (true) {
        std::string line;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });

            if (m_stopping && m_queue.empty()) {
                break;
            }

            line = m_queue.front();
            m_queue.pop();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized && m_file.is_open()) {
            m_file << line << std::endl;
            m_file.flush();
        }
    }
}
