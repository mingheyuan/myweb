#ifndef CONFIG_H
#define CONFIG_H

class Config {
public:
    Config();
    ~Config() = default;

    void parse_arg(int argc,char* argv[]);

public:
    int PORT;
};

#endif