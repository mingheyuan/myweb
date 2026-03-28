#ifndef LOGGER_H
#define LOGGER_H

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class Logger {
public:
    static Logger &instance();

    bool init(const std::string &file_path, bool async_mode);
    void shutdown();

    void info(const std::string &msg);
    void warn(const std::string &msg);
    void error(const std::string &msg);

private:
    Logger();
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void write_line(const std::string &level, const std::string &msg);
    std::string build_line(const std::string &level, const std::string &msg) const;
    void worker_loop();

private:
    std::ofstream m_file;
    bool m_initialized;
    bool m_async_mode;
    bool m_stopping;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::string> m_queue;
    std::thread m_worker;
};

#endif
