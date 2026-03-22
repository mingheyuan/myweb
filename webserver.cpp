#include "webserver.h"
#include <iostream>

WebServer::WebServer() : m_port(9006) {}

WebServer::~WebServer() = default;

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
    std::cout << "[listen] not implemented yet" << std::endl;
}

void WebServer::eventLoop() {
    std::cout << "[loop] skeleton ready, exit now" << std::endl;
}