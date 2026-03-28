#include "webserver.h"
#include "http_conn.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>

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

bool add_epollin_fd(int epollfd, int fd)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = EPOLLIN;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
        return false;
    return set_nonblocking(fd) != -1;
}

void remove_fd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}
} // namespace

WebServer::WebServer() : m_port(9006), m_listenfd(-1), m_epollfd(-1) {}

WebServer::~WebServer() {
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
}

void WebServer::log_write() {
    std::cout << "[log] not implemented yet" << std::endl;
}

void WebServer::sql_pool() {
    std::cout << "[sql] not implemented yet" << std::endl;
}

void WebServer::thread_pool() {
    std::cout << "[threadpool] not implemented yet" << std::endl;
}

void WebServer::trig_mode() {
    std::cout << "[trig_mode] stage uses LT" << std::endl;
}

void WebServer::eventListen() {
    m_listenfd =socket(AF_INET,SOCK_STREAM,0);
    if (m_listenfd <0) {
        std::perror("socket");
        m_listenfd = -1;
        return;
    }

    int reuse =1;
    if (setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))< 0) {
        std::perror("setsockopt");
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
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    if (listen(m_listenfd,8)<0) {
        std::perror("listen");
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    m_epollfd = epoll_create1(0);
    if (m_epollfd < 0) {
        std::perror("epoll_create1");
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    if (!add_epollin_fd(m_epollfd, m_listenfd)) {
        std::perror("add listenfd to epoll");
        close(m_epollfd);
        m_epollfd = -1;
        close(m_listenfd);
        m_listenfd = -1;
        return;
    }

    std::cout<<"[listen] 0.0.0.0:"<<m_port<<std::endl;
}

void WebServer::eventLoop() {
    if (m_listenfd <0 || m_epollfd < 0) {
        std::cerr << "[loop] listen/epoll fd invalid" << std::endl;
        return;
    }

    epoll_event events[kMaxEvents];
    HttpConn http_conn;
    std::cout << "[loop] epoll LT waiting events..." <<std::endl;

    while (true) {
        int event_cnt = epoll_wait(m_epollfd, events, kMaxEvents, -1);
        if (event_cnt < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("epoll_wait");
            break;
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
                    }
                    continue;
                }

                if (!add_epollin_fd(m_epollfd, connfd)) {
                    std::perror("add connfd to epoll");
                    close(connfd);
                    continue;
                }

                std::cout << "[accept] " << inet_ntoa(client.sin_addr)
                          << ":" << ntohs(client.sin_port)
                          << " fd=" << connfd << std::endl;
                continue;
            }

            if ((events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                remove_fd(m_epollfd, fd);
                continue;
            }

            if ((events[i].events & EPOLLIN) == 0) {
                continue;
            }

            char buf[2048];
            ssize_t nread = recv(fd, buf, sizeof(buf), 0);
            if (nread <= 0) {
                remove_fd(m_epollfd, fd);
                continue;
            }

            std::string request(buf, static_cast<std::size_t>(nread));
            std::string response = http_conn.process(request);

            ssize_t nsend = send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
            if (nsend < 0) {
                std::perror("send");
            }

            remove_fd(m_epollfd, fd);
        }
    }
}