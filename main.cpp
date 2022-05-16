#include "config.h"
int main(int argc, char *argv[])
{
    std::string user{"root"};
    std::string password{"1215"};
    std::string database_name{"Yo_Web_db"};

    Config config;

    config.parse_arg(argc, argv);

    WebServer server;

    server.init(config.PORT, user, password, database_name, config.log_write,
                config.opt_linger, config.trigger_mode, config.sql_pool_num, config.thread_pool_num,
                config.is_close_log, config.concurrence__actor_model);

    server.logwrite();

    server.sql_pool();

    server.thread_pool();

    server.trigger_mode();

    server.eventListen();

    /*ÔËÐÐ*/
    server.eventLoop();
    exit(0);
}