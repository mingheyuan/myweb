#include "config.h"
#include "webserver.h"
#include <iostream>

int main(int argc,char* argv[]) {
    Config config;
    config.parse_arg(argc,argv);

    WebServer server;
    server.init(config.PORT,
                config.LOGWrite,
                config.close_log,
                config.thread_num,
                config.timeout_sec,
                config.sql_num,
                config.TRIGMode,
                config.actor_model,
                config.db_host,
                config.db_user,
                config.db_password,
                config.db_name,
                config.db_port);
    server.log_write();
    server.sql_pool();
    server.thread_pool();
    server.trig_mode();
    server.eventListen();
    server.eventLoop();

    return 0;
}