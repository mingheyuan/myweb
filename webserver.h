#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "http_conn.h"
#include "sql_connection_pool.h"
#include "timer.h"
#include "threadpool.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port,
              int log_write,
              int close_log,
              int thread_num,
              int timeout_sec,
              int sql_num,
              int trig_mode,
              int actor_model,
              const char *db_host,
              const char *db_user,
              const char *db_password,
              const char *db_name,
              int db_port);
    void log_write();
    void sql_pool();
    void thread_pool();
    void trig_mode();
    void eventListen();
    void eventLoop();

private:
    int m_port;
    int m_log_write;
    int m_close_log;
    int m_thread_num;
    int m_sql_num;
    int m_trig_mode;
    int m_listen_trig_mode;
    int m_conn_trig_mode;
    int m_actor_model;
    std::string m_db_host;
    std::string m_db_user;
    std::string m_db_password;
    std::string m_db_name;
    int m_db_port;
    int m_listenfd;
    int m_epollfd;
    std::unique_ptr<ThreadPool> m_pool;
    std::unordered_map<int, std::shared_ptr<HttpConn> > m_users;
    std::mutex m_users_mutex;
    TimerManager m_timer;
    int m_timeout_sec;
};

#endif