#include "webserver.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>

WebServer::WebServer() : m_port(9006), m_listenfd(-1) {}

WebServer::~WebServer() {
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
    std::cout << "[trig_mode] not implemented yet" << std::endl;
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

    std::cout<<"[listen] 0.0.0.0:"<<m_port<<std::endl;
}

void WebServer::eventLoop() {
    if (m_listenfd <0) {
        std::cerr << "[loop] listen fd invalid" << std::endl;
        return;
    }

    static const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: 28\r\n"
        "\r\n"
        "Hello TinyWebServer Stage1!\n";

    std::cout << "[loop] waiting connections..." <<std::endl;

    while (true) {
        sockaddr_in client;
        socklen_t len = sizeof(client);
        int connfd =accept(m_listenfd,reinterpret_cast<sockaddr*>(&client),&len);
        if (connfd <0) {
            std::perror("accept");
            if (errno == EINTR) {
                continue;
            }
            if (errno == EINVAL || errno == EBADF || errno == ENOTSOCK) {
                std::cerr << "[loop] listener is not in a valid state, stop loop" << std::endl;
                break;
            }
            continue;
        }

        std::cout   << "[accept]" << inet_ntoa(client.sin_addr)
                    <<":"<<ntohs(client.sin_port)<<std::endl;

        ssize_t n =send(connfd,response,sizeof(response)-1,0);
        if( n<0 ) {
            std::perror("send");
        }

        close(connfd);
    }
}