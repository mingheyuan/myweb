#ifndef WEBSERVER_H
#define WEBSERVER_H

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
};

#endif