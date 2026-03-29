#include "webserver.h"
#include "logger.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
const int kMaxEvents = 10000;

int set_nonblocking(int fd)
{
    int old_flags = fcntl(fd, F_GETFL, 0);
    if (old_flags == -1)
        return -1;
    if (fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) == -1)
        return -1;
    return old_flags;
}

bool add_epollin_fd(int epollfd, int fd, bool one_shot, bool use_et)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (use_et) {
        ev.events |= EPOLLET;
    }
    if (one_shot) {
        ev.events |= EPOLLONESHOT;
    }

    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) != -1 && set_nonblocking(fd) != -1;
}

bool mod_epollin_fd(int epollfd, int fd, bool one_shot, bool use_et)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (use_et) {
        ev.events |= EPOLLET;
    }
    if (one_shot) {
        ev.events |= EPOLLONESHOT;
    }

    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev) != -1;
}

void remove_fd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
}
} // namespace

WebServer::WebServer()
        : m_port(9006),
            m_log_write(0),
            m_close_log(0),
            m_thread_num(4),
            m_sql_num(4),
            m_trig_mode(0),
            m_listen_trig_mode(0),
            m_conn_trig_mode(0),
            m_actor_model(0),
            m_db_host("127.0.0.1"),
            m_db_user("root"),
            m_db_password("123456"),
            m_db_name("yourdb"),
            m_db_port(3306),
            m_listenfd(-1),
            m_epollfd(-1),
            m_timeout_sec(15) {}

WebServer::~WebServer() {
    SqlConnectionPool::instance().destroy();
    Logger::instance().shutdown();
    if (m_epollfd != -1) {
        close(m_epollfd);
        m_epollfd = -1;
    }
    if (m_listenfd !=-1) {
        close(m_listenfd);
        m_listenfd =-1;
    }
}

void WebServer::init(int port,
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
                     int db_port) {
    m_port = port;
    m_log_write = log_write;
    m_close_log = close_log;
    m_thread_num = thread_num > 0 ? thread_num : 4;
    m_timeout_sec = timeout_sec > 0 ? timeout_sec : 15;
    m_sql_num = sql_num;
    m_trig_mode = (trig_mode >= 0 && trig_mode <= 3) ? trig_mode : 0;
    m_actor_model = (actor_model == 1) ? 1 : 0;

    if (m_trig_mode == 0) {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 0;
    } else if (m_trig_mode == 1) {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 1;
    } else if (m_trig_mode == 2) {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 0;
    } else {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 1;
    }
    if (db_host && db_host[0] != '\0') {
        m_db_host = db_host;
    }
    if (db_user && db_user[0] != '\0') {
        m_db_user = db_user;
    }
    if (db_password && db_password[0] != '\0') {
        m_db_password = db_password;
    }
    if (db_name && db_name[0] != '\0') {
        m_db_name = db_name;
    }
    m_db_port = db_port > 0 ? db_port : 3306;
    std::cout << "[init] port = " << m_port << std::endl;
}

void WebServer::log_write() {
    if (m_close_log == 1) {
        Logger::instance().set_silent(true);
        std::cout << "[log] disabled by -c 1" << std::endl;
        return;
    }

    Logger::instance().set_silent(false);
    bool async_mode = (m_log_write == 1);
    if (Logger::instance().init("./server.log", async_mode)) {
        Logger::instance().info(async_mode ? "logger initialized in async mode" : "logger initialized in sync mode");
        Logger::instance().info(std::string("server init, port=") + std::to_string(m_port));
    } else {
        std::cerr << "[log] init failed, fallback to stderr" << std::endl;
    }
}

void WebServer::sql_pool() {
    if (m_sql_num <= 0) {
        std::cout << "[sql] disabled by config (-s 0)" << std::endl;
        Logger::instance().info("sql pool disabled by config");
        return;
    }

    bool ok = SqlConnectionPool::instance().init(m_db_host, m_db_user, m_db_password, m_db_name, m_db_port, m_sql_num);
    if (ok) {
        std::cout << "[sql] pool initialized, size=" << m_sql_num << std::endl;
        Logger::instance().info(std::string("sql pool initialized, size=") + std::to_string(m_sql_num));
    } else {
        std::cout << "[sql] pool init failed, db features disabled" << std::endl;
        Logger::instance().warn("sql pool init failed, db features disabled");
    }
}

void WebServer::thread_pool() {
    m_pool.reset(new ThreadPool(static_cast<std::size_t>(m_thread_num)));
    std::cout << "[threadpool] started with " << m_thread_num << " workers" << std::endl;
    Logger::instance().info(std::string("thread pool started with ") + std::to_string(m_thread_num) + " workers");
}

void WebServer::trig_mode() {
    std::cout << "[trig_mode] mode=" << m_trig_mode
              << " listen=" << (m_listen_trig_mode ? "ET" : "LT")
              << " conn=" << (m_conn_trig_mode ? "ET" : "LT")
              << " actor=" << (m_actor_model ? "Reactor" : "Proactor")
              << std::endl;
}

void WebServer::eventListen() {
    m_listenfd =socket(AF_INET,SOCK_STREAM,0);
    if (m_listenfd <0) {
        std::perror("socket");
        Logger::instance().error(std::string("socket failed: ") + std::strerror(errno));
        m_listenfd = -1;
        return;
    }

    int reuse =1;
    if (setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))< 0) {
        std::perror("setsockopt");
        Logger::instance().error(std::string("setsockopt failed: ") + std::strerror(errno));
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    // Best-effort: improve accept scalability under high concurrency.
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr,0,sizeof(addr));
    addr.sin_family =AF_INET;
    addr.sin_addr.s_addr =htonl(INADDR_ANY);
    addr.sin_port =htons(m_port);

    if (bind(m_listenfd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0) {
        std::perror("bind");
        Logger::instance().error(std::string("bind failed: ") + std::strerror(errno));
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    if (listen(m_listenfd,SOMAXCONN)<0) {
        std::perror("listen");
        Logger::instance().error(std::string("listen failed: ") + std::strerror(errno));
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    m_epollfd = epoll_create1(0);
    if (m_epollfd < 0) {
        std::perror("epoll_create1");
        Logger::instance().error(std::string("epoll_create1 failed: ") + std::strerror(errno));
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    if (!add_epollin_fd(m_epollfd, m_listenfd, false, m_listen_trig_mode == 1)) {
        std::perror("add listenfd to epoll");
        Logger::instance().error(std::string("add listenfd to epoll failed: ") + std::strerror(errno));
        close(m_epollfd);
        m_epollfd = -1;
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    std::cout<<"[listen] 0.0.0.0:"<<m_port<<std::endl;
    Logger::instance().info(std::string("listen on 0.0.0.0:") + std::to_string(m_port));
}

void WebServer::eventLoop() {
    if (m_listenfd <0 || m_epollfd < 0) {
        std::cerr << "[loop] listen/epoll fd invalid" << std::endl;
        Logger::instance().error("event loop start failed: listen/epoll fd invalid");
        return;
    }

    epoll_event events[kMaxEvents];
    std::cout << "[loop] epoll LT waiting events..." <<std::endl;

    while (true) {
        int event_cnt = epoll_wait(m_epollfd, events, kMaxEvents, 1000);
        if (event_cnt < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("epoll_wait");
            Logger::instance().error(std::string("epoll_wait failed: ") + std::strerror(errno));
            break;
        }

        std::time_t now = std::time(nullptr);
        std::vector<int> expired = m_timer.collect_expired(now, m_timeout_sec);
        for (std::size_t i = 0; i < expired.size(); ++i) {
            int expired_fd = expired[i];
            std::shared_ptr<HttpConn> conn;
            {
                std::lock_guard<std::mutex> lock(m_users_mutex);
                std::unordered_map<int, std::shared_ptr<HttpConn> >::iterator it = m_users.find(expired_fd);
                if (it != m_users.end()) {
                    conn = it->second;
                    m_users.erase(it);
                }
            }

            if (conn) {
                conn->close_conn();
                Logger::instance().warn(std::string("idle timeout close fd=") + std::to_string(expired_fd));
            }
            remove_fd(m_epollfd, expired_fd);
        }

        for (int i = 0; i < event_cnt; ++i) {
            int fd = events[i].data.fd;

            if (fd == m_listenfd) {
                while (true) {
                    sockaddr_in client;
                    socklen_t len = sizeof(client);
                    int connfd = accept(m_listenfd, reinterpret_cast<sockaddr *>(&client), &len);
                    if (connfd < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                            std::perror("accept");
                            Logger::instance().error(std::string("accept failed: ") + std::strerror(errno));
                        }
                        break;
                    }

                    if (!add_epollin_fd(m_epollfd, connfd, true, m_conn_trig_mode == 1)) {
                        std::perror("add connfd to epoll");
                        Logger::instance().error(std::string("add connfd to epoll failed: ") + std::strerror(errno));
                        close(connfd);
                        continue;
                    }

                    std::shared_ptr<HttpConn> conn(new HttpConn());
                    conn->init(connfd, client);
                    {
                        std::lock_guard<std::mutex> lock(m_users_mutex);
                        m_users[connfd] = conn;
                    }
                    m_timer.add(connfd, now);

                    if (m_close_log == 0) {
                        std::ostringstream oss;
                        oss << "accept " << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port) << " fd=" << connfd;
                        Logger::instance().info(oss.str());
                    }

                    if (m_listen_trig_mode == 0) {
                        break;
                    }
                }
                continue;
            }

            std::shared_ptr<HttpConn> conn;
            {
                std::lock_guard<std::mutex> lock(m_users_mutex);
                std::unordered_map<int, std::shared_ptr<HttpConn> >::iterator it = m_users.find(fd);
                if (it != m_users.end()) {
                    conn = it->second;
                }
            }

            if (!conn) {
                remove_fd(m_epollfd, fd);
                close(fd);
                continue;
            }

            if ((events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                conn->close_conn();
                {
                    std::lock_guard<std::mutex> lock(m_users_mutex);
                    m_users.erase(fd);
                }
                remove_fd(m_epollfd, fd);
                continue;
            }

            if ((events[i].events & EPOLLIN) == 0) {
                continue;
            }

            if (!m_pool) {
                std::cerr << "[threadpool] not initialized" << std::endl;
                Logger::instance().error("thread pool not initialized");
                conn->close_conn();
                {
                    std::lock_guard<std::mutex> lock(m_users_mutex);
                    m_users.erase(fd);
                }
                m_timer.remove(fd);
                remove_fd(m_epollfd, fd);
                continue;
            }

            if (m_actor_model == 0) {
                if (!conn->read_once()) {
                    Logger::instance().warn(std::string("read failed or peer closed, fd=") + std::to_string(fd));
                    conn->close_conn();
                    {
                        std::lock_guard<std::mutex> lock(m_users_mutex);
                        m_users.erase(fd);
                    }
                    m_timer.remove(fd);
                    remove_fd(m_epollfd, fd);
                    continue;
                }
                m_timer.touch(fd, now);
            }

            m_pool->enqueue([this, fd]() {
                std::shared_ptr<HttpConn> task_conn;
                {
                    std::lock_guard<std::mutex> lock(m_users_mutex);
                    std::unordered_map<int, std::shared_ptr<HttpConn> >::iterator it = m_users.find(fd);
                    if (it == m_users.end()) {
                        return;
                    }
                    task_conn = it->second;
                }

                if (m_actor_model == 1) {
                    if (!task_conn->read_once()) {
                        task_conn->close_conn();
                        remove_fd(m_epollfd, fd);
                        m_timer.remove(fd);
                        std::lock_guard<std::mutex> lock(m_users_mutex);
                        m_users.erase(fd);
                        return;
                    }
                    m_timer.touch(fd, std::time(nullptr));
                }

                HttpConn::HTTP_CODE code = task_conn->process();
                if (code == HttpConn::NO_REQUEST) {
                    if (!mod_epollin_fd(m_epollfd, fd, true, m_conn_trig_mode == 1)) {
                        Logger::instance().warn(std::string("rearm EPOLLONESHOT failed, close fd=") + std::to_string(fd));
                        task_conn->close_conn();
                        remove_fd(m_epollfd, fd);
                        m_timer.remove(fd);
                        std::lock_guard<std::mutex> lock(m_users_mutex);
                        m_users.erase(fd);
                    }
                    return;
                }

                if (!task_conn->write()) {
                    std::perror("write");
                    Logger::instance().warn(std::string("write failed, fd=") + std::to_string(fd));
                }

                task_conn->close_conn();
                remove_fd(m_epollfd, fd);
                m_timer.remove(fd);
                std::lock_guard<std::mutex> lock(m_users_mutex);
                m_users.erase(fd);
            });
        }
    }
}