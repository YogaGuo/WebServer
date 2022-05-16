/*
 * @Description:
 * @Version: 2.0
 * @Autor: Yogaguo
 * @Date: 2022-05-12 22:03:32
 * @LastEditors: Yogaguo
 * @LastEditTime: 2022-05-15 10:57:33
 */
#ifndef _WEBSERVER__H
#define _WEBSERVER__H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <string>
#include <string.h>
#include <syslog.h>
#include "thread_pool.h"
#include "./http/http_conn.h"
#include "./time/sort_timer_lst.h"
const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位
class WebServer
{
public:
    WebServer();
    ~WebServer();
    void eventListen();
    void eventLoop();

    void init(int port, const std::string &user, const std::string &passWord, const std::string &databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void trigger_mode();

    bool deal_client();

    void deal_read(int fd);

    void deal_write(int fd);

    bool deal_signal(bool &timeout, bool &stop_server);

    void timer(int connfd, struct sockaddr_in &client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);

    void thread_pool();

public:
    int m_epollfd;

    /*通信管道*/
    int m_pipe[2];
    //用于接收一个用户连接
    http_conn *users;

    thread_pool<http_conn> *m_pool;
    int m_thread_num;

    //定时器相关
    client_data *users_timer;

    Utils utils;

    /*数据库相关*/
    connection *m_conn_pool;
    std::string m_user;
    std::string m_password;
    std::string m_database_name;
    int m_sql_num;

    /*监听端口 */
    int m_port;

    char *m_root;

    static int m_listenfd;
    /*日志选项*/
    int m_log_write;
    int m_close_log;

    /*触发模式 reactor proactor*/
    int m_actor_mode;

    /*触发模式： ET+LT LT+LT ET+LT LT+ET*/
    int m_trigger_mode;
    int m_listen_tri_mode;
    int m_connfd_tri_mode;
    int m_opt_linger;
};
#endif