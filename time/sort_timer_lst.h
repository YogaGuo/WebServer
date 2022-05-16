
#ifndef LIST_TIMER
#define LIST_TIMER
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include "../http/http_conn.h"
#define LST_TIMER
#define BUF_SIZE 64
class util_timer; /*前向声明*/
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUF_SIZE];
    util_timer *timer;
};

/*定时器*/

class util_timer
{
public:
    util_timer()
        : prev(nullptr), next(nullptr){};

public:
    time_t expire;                  /*任务的超时时间 这里是绝对时间*/
    void (*cb_func)(client_data *); /*任务回调*/
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

/*定时器链表 升序 双向 */
class sort_timer_lst
{
public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {}

    /*链表被销毁时，删除其中所有*/

    ~sort_timer_lst();

    /*将目标定时器加入链表*/
    void add_timer(util_timer *timer);

    /*当某个定时任务发生变化， 调整对应定时器位置 注意此函数只考虑被调整定时器超时时间延长时间*/

    void adjust_timer(util_timer *timer);

    /*将定时器timer从链表中删除*/

    void del_timer(util_timer *timer);

    /*SIGALAM 信号每次被触发就在其信号 处理函数中执行一次 tick 函数， 以处理链表上的任务*/
    void tick();

private:
    util_timer *head;
    util_timer *tail;

    /*重载的辅助函数， 将目标定时器 timer 添加到结点 lst_head之后的部分链表中*/
    void add_timer(util_timer *timer, util_timer *lst_head);
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif
