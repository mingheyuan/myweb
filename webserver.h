#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "http_conn.h"
#include "timer.h"
#include "threadpool.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port);
    void log_write();
    void sql_pool();
    void thread_pool();
    void trig_mode();
    void eventListen();
    void eventLoop();

private:
    int m_port;
    int m_listenfd;
    int m_epollfd;
    std::unique_ptr<ThreadPool> m_pool;
    std::unordered_map<int, std::shared_ptr<HttpConn> > m_users;
    std::mutex m_users_mutex;
    TimerManager m_timer;
    int m_timeout_sec;
};

#endif