#include "sql_connection_pool.h"

#include <iostream>

SqlConnectionPool &SqlConnectionPool::instance()
{
    static SqlConnectionPool pool;
    return pool;
}

SqlConnectionPool::SqlConnectionPool() : m_initialized(false), m_max_conn(0)
{
}

SqlConnectionPool::~SqlConnectionPool()
{
    destroy();
}

bool SqlConnectionPool::init(const std::string &host,
                             const std::string &user,
                             const std::string &password,
                             const std::string &db_name,
                             int port,
                             int max_conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return true;
    }

    if (max_conn <= 0) {
        max_conn = 4;
    }

    for (int i = 0; i < max_conn; ++i) {
        MYSQL *conn = mysql_init(nullptr);
        if (!conn) {
            destroy();
            return false;
        }

        unsigned int timeout_sec = 2;
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
        mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout_sec);
        mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout_sec);

        if (!mysql_real_connect(conn,
                                host.c_str(),
                                user.c_str(),
                                password.c_str(),
                                db_name.c_str(),
                                static_cast<unsigned int>(port),
                                nullptr,
                                0)) {
            mysql_close(conn);
            destroy();
            return false;
        }

        m_conn_queue.push(conn);
    }

    m_max_conn = max_conn;
    m_initialized = true;
    return true;
}

MYSQL *SqlConnectionPool::get_connection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        return nullptr;
    }

    m_cv.wait(lock, [this]() { return !m_conn_queue.empty(); });

    MYSQL *conn = m_conn_queue.front();
    m_conn_queue.pop();
    return conn;
}

void SqlConnectionPool::release_connection(MYSQL *conn)
{
    if (!conn) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) {
            mysql_close(conn);
            return;
        }
        m_conn_queue.push(conn);
    }
    m_cv.notify_one();
}

void SqlConnectionPool::destroy()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_conn_queue.empty()) {
        MYSQL *conn = m_conn_queue.front();
        m_conn_queue.pop();
        mysql_close(conn);
    }
    m_initialized = false;
    m_max_conn = 0;
}

bool SqlConnectionPool::is_ready() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

SqlConnRAII::SqlConnRAII(MYSQL **sql) : m_conn(nullptr)
{
    m_conn = SqlConnectionPool::instance().get_connection();
    if (sql) {
        *sql = m_conn;
    }
}

SqlConnRAII::~SqlConnRAII()
{
    SqlConnectionPool::instance().release_connection(m_conn);
}
