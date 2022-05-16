#ifndef _CONFIG_H
#define _CONFIG_H

#include "Webserver.h"
class Config
{
public:
    Config();
    ~Config();

    void parse_arg(int argc, char *argv[]);

    int PORT;

    int log_write;

    /*触发组合模式*/
    int trigger_mode;

    int listenfd_trigger_mode;

    int connfd_trigger_mode;

    int opt_linger;

    int sql_pool_num;

    int thread_pool_num;

    int concurrence__actor_model;

    int is_close_log;
};
#endif