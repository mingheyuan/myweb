#include "config.h"
#include "webserver.h"
#include <iostream>

int main(int argc,char* argv[]) {
    Config config;
    config.parse_arg(argc,argv);

    WebServer server;
    server.init(config.PORT);
    server.log_write();
    server.sql_pool();
    server.thread_pool();
    server.trig_mode();
    server.eventListen();
    server.eventLoop();

    return 0;
}