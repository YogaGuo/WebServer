
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
class util_timer; /*ǰ������*/
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUF_SIZE];
    util_timer *timer;
};

/*��ʱ��*/

class util_timer
{
public:
    util_timer()
        : prev(nullptr), next(nullptr){};

public:
    time_t expire;                  /*����ĳ�ʱʱ�� �����Ǿ���ʱ��*/
    void (*cb_func)(client_data *); /*����ص�*/
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

/*��ʱ������ ���� ˫�� */
class sort_timer_lst
{
public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {}

    /*��������ʱ��ɾ����������*/

    ~sort_timer_lst();

    /*��Ŀ�궨ʱ����������*/
    void add_timer(util_timer *timer);

    /*��ĳ����ʱ�������仯�� ������Ӧ��ʱ��λ�� ע��˺���ֻ���Ǳ�������ʱ����ʱʱ���ӳ�ʱ��*/

    void adjust_timer(util_timer *timer);

    /*����ʱ��timer��������ɾ��*/

    void del_timer(util_timer *timer);

    /*SIGALAM �ź�ÿ�α������������ź� ��������ִ��һ�� tick ������ �Դ��������ϵ�����*/
    void tick();

private:
    util_timer *head;
    util_timer *tail;

    /*���صĸ��������� ��Ŀ�궨ʱ�� timer ��ӵ���� lst_head֮��Ĳ���������*/
    void add_timer(util_timer *timer, util_timer *lst_head);
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //���ļ����������÷�����
    int setnonblocking(int fd);

    //���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //�źŴ�����
    static void sig_handler(int sig);

    //�����źź���
    void addsig(int sig, void(handler)(int), bool restart = true);

    //��ʱ�����������¶�ʱ�Բ��ϴ���SIGALRM�ź�
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
