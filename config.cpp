#include "config.h"
#include <unistd.h>
#include <cstdlib>

Config::Config() : PORT(9006) {}

void Config::parse_arg(int argc,char* argv[]){
    int opt;
    while ((opt =getopt(argc,argv,"p:"))!=-1) {
        if(opt=='p') {
            PORT =std::atoi(optarg);
        }
    }
}