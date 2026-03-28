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
const int kMaxEvents = 1024;

int set_nonblocking(int fd)
{
    int old_flags = fcntl(fd, F_GETFL, 0);
    if (old_flags == -1)
        return -1;
    if (fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) == -1)
        return -1;
    return old_flags;
}

bool add_epollin_fd(int epollfd, int fd, bool one_shot)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if (one_shot) {
        ev.events |= EPOLLONESHOT;
    }

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
        return false;
    return set_nonblocking(fd) != -1;
}

bool mod_epollin_fd(int epollfd, int fd, bool one_shot)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
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

WebServer::WebServer() : m_port(9006), m_listenfd(-1), m_epollfd(-1), m_timeout_sec(15) {}

WebServer::~WebServer() {
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

void WebServer::init(int port) {
    m_port = port;
    std::cout << "[init] port = " << m_port << std::endl;
    Logger::instance().info(std::string("server init, port=") + std::to_string(m_port));
}

void WebServer::log_write() {
    if (Logger::instance().init("./server.log", false)) {
        Logger::instance().info("logger initialized in sync mode");
    } else {
        std::cerr << "[log] init failed, fallback to stderr" << std::endl;
    }
}

void WebServer::sql_pool() {
    std::cout << "[sql] not implemented yet" << std::endl;
}

void WebServer::thread_pool() {
    m_pool.reset(new ThreadPool(4));
    std::cout << "[threadpool] started with 4 workers" << std::endl;
    Logger::instance().info("thread pool started with 4 workers");
}

void WebServer::trig_mode() {
    std::cout << "[trig_mode] stage uses LT" << std::endl;
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

    if (listen(m_listenfd,8)<0) {
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

    if (!add_epollin_fd(m_epollfd, m_listenfd, false)) {
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
                sockaddr_in client;
                socklen_t len = sizeof(client);
                int connfd = accept(m_listenfd, reinterpret_cast<sockaddr *>(&client), &len);
                if (connfd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                        std::perror("accept");
                        Logger::instance().error(std::string("accept failed: ") + std::strerror(errno));
                    }
                    continue;
                }

                if (!add_epollin_fd(m_epollfd, connfd, true)) {
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

                std::cout << "[accept] " << inet_ntoa(client.sin_addr)
                          << ":" << ntohs(client.sin_port)
                          << " fd=" << connfd << std::endl;
                {
                    std::ostringstream oss;
                    oss << "accept " << inet_ntoa(client.sin_addr) << ":" << ntohs(client.sin_port) << " fd=" << connfd;
                    Logger::instance().info(oss.str());
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

                HttpConn::HTTP_CODE code = task_conn->process();
                if (code == HttpConn::NO_REQUEST) {
                    if (!mod_epollin_fd(m_epollfd, fd, true)) {
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