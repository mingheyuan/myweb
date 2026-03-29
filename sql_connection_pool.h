#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

#include <mysql/mysql.h>

class SqlConnectionPool {
public:
    static SqlConnectionPool &instance();

    bool init(const std::string &host,
              const std::string &user,
              const std::string &password,
              const std::string &db_name,
              int port,
              int max_conn);

    MYSQL *get_connection();
    void release_connection(MYSQL *conn);
    void destroy();

    bool is_ready() const;

private:
    SqlConnectionPool();
    ~SqlConnectionPool();

    SqlConnectionPool(const SqlConnectionPool &) = delete;
    SqlConnectionPool &operator=(const SqlConnectionPool &) = delete;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<MYSQL *> m_conn_queue;
    bool m_initialized;
    int m_max_conn;
};

class SqlConnRAII {
public:
    explicit SqlConnRAII(MYSQL **sql);
    ~SqlConnRAII();

private:
    MYSQL *m_conn;
};

#endif
