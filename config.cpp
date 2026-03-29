#include "config.h"
#include <unistd.h>
#include <cstdlib>

Config::Config()
    : PORT(9006),
      LOGWrite(0),
      close_log(0),
      thread_num(4),
      timeout_sec(15),
      sql_num(4),
            TRIGMode(0),
            actor_model(0),
      db_port(3306),
      db_host("127.0.0.1"),
      db_user("root"),
      db_password("123456"),
      db_name("yourdb") {}

void Config::parse_arg(int argc,char* argv[]){
    int opt;
        while ((opt =getopt(argc,argv,"p:l:c:t:T:s:m:a:H:U:W:D:P:"))!=-1) {
        if(opt=='p') {
            PORT =std::atoi(optarg);
        } else if (opt == 'l') {
            LOGWrite = std::atoi(optarg);
        } else if (opt == 'c') {
            close_log = std::atoi(optarg);
        } else if (opt == 't') {
            thread_num = std::atoi(optarg);
            if (thread_num <= 0) {
                thread_num = 4;
            }
        } else if (opt == 'T') {
            timeout_sec = std::atoi(optarg);
            if (timeout_sec <= 0) {
                timeout_sec = 15;
            }
        } else if (opt == 's') {
            sql_num = std::atoi(optarg);
            if (sql_num < 0) {
                sql_num = 4;
            }
        } else if (opt == 'm') {
            TRIGMode = std::atoi(optarg);
            if (TRIGMode < 0 || TRIGMode > 3) {
                TRIGMode = 0;
            }
        } else if (opt == 'a') {
            actor_model = std::atoi(optarg);
            if (actor_model != 0 && actor_model != 1) {
                actor_model = 0;
            }
        } else if (opt == 'H') {
            db_host = optarg;
        } else if (opt == 'U') {
            db_user = optarg;
        } else if (opt == 'W') {
            db_password = optarg;
        } else if (opt == 'D') {
            db_name = optarg;
        } else if (opt == 'P') {
            db_port = std::atoi(optarg);
            if (db_port <= 0) {
                db_port = 3306;
            }
        }
    }
}