#include "config.h"

Config::Config()
{
    PORT = 9007;

    /*默认同步*/
    log_write = 0;

    /*默认listefd LT + connfd LT*/
    trigger_mode = 0;

    /*默认LT*/
    listenfd_trigger_mode = 0;

    connfd_trigger_mode = 0;

    /*默认不用*/
    opt_linger = 0;

    sql_pool_num = 8;

    thread_pool_num = 8;

    /*默认不关闭*/
    is_close_log = 0;
}

void Config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
            PORT = atoi(optarg);
            break;
        case 'l':
            log_write = atoi(optarg);
            break;
        case 'm':
            trigger_mode = atoi(optarg);
            break;
        case 'o':
            opt_linger = atoi(optarg);
            break;
        case 's':
            sql_pool_num = atoi(optarg);
            break;
        case 't':
            thread_pool_num = atoi(optarg);
            break;
        case 'c':
            is_close_log = atoi(optarg);
            break;
        case 'a':
            concurrence__actor_model = atoi(optarg);
            break;
        default:
            break;
        }
    }
}