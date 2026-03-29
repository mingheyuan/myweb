#ifndef CONFIG_H
#define CONFIG_H

class Config {
public:
    Config();
    ~Config() = default;

    void parse_arg(int argc,char* argv[]);

public:
    int PORT;
    int LOGWrite;
    int close_log;
    int thread_num;
    int timeout_sec;
    int sql_num;
    int TRIGMode;
    int actor_model;
    int db_port;
    const char *db_host;
    const char *db_user;
    const char *db_password;
    const char *db_name;
};

#endif