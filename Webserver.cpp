#include "Webserver.h"
WebServer::WebServer()
{

    users = new http_conn[MAX_FD];

    /*root 文件夹路径*/
    char server_path[200];
    getwd(server_path);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    /*定时器*/
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipe[0]);
    close(m_pipe[1]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}
void WebServer::trigger_mode()
{
    /*LT+LT*/
    if (0 == m_trigger_mode)
    {
        m_listen_tri_mode = 0;
        m_connfd_tri_mode = 0;
    }
    /*LT+ET*/
    else if (1 == m_trigger_mode)
    {
        m_listen_tri_mode = 0;
        m_connfd_tri_mode = 1;
    }
    else if (2 == m_trigger_mode)
    {
        m_listen_tri_mode = 1;
        m_connfd_tri_mode = 0;
    }
    else if (3 == m_trigger_mode)
    {
        m_listen_tri_mode = 1;
        m_connfd_tri_mode = 1;
    }
}

void WebServer::thread_pool()
{
    m_pool = new thread_pool<http_conn>(m_actor_mode, m_connfd_tri_mode, m_thread_num);
}

void WebServer::eventListen()
{
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    if (0 == m_opt_linger)
    {
        struct linger tmp
        {
            0, 1
        };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    else if (1 == m_opt_linger)
    {
        struct linger tmp
        {
            1, 1
        };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    struct sockaddr_in address;

    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ret = 0;
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(m_listenfd, 5);
    assert(ret != -1);

    utils.init(TIMESLOT);
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    utils.addfd(m_epollfd, m_listenfd, false, m_listen_tri_mode);

    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipe);
    assert(ret != -1);

    utils.setnonblocking(m_pipe[1]);

    /*设置管道读端ET+非阻塞*/
    utils.addfd(m_epollfd, m_pipe[0], false, 0);
    utils.addsig(SIGPIPE, SIG_IGN);

    /*传递给主循环的信号*/
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    /*把控制定时器的管和主调管道统一*/
    Utils::u_pipefd = m_pipe;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in &client_addr)
{
    users[connfd].init(connfd, client_addr, m_root, m_connfd_tri_mode, m_close_log,
                       m_user, m_password, m_database_name);

    users_timer[connfd].addr = client_addr;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
}

bool WebServer::deal_client()
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    client_addr_len = sizeof(client_addr);

    if (m_listen_tri_mode == 0)
    {
        int conn_fd = accept(m_listenfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn_fd < 0)
        {
            LOG_ERROR("%s : error is: %d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(conn_fd, "Internal server busy");
            LOG_ERR("%s", "Internal server busy");
            return false;
        }
        timer(conn_fd, client_addr);
    }
    else
    {
        while (1)
        {
            int conn_fd = accept(m_listenfd, (struct sockaddr *)&client_addr, sizeof(client_addr_len));
            if (conn_fd < 0)
            {
                LOG_ERROR("%s : error is: %d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(conn_fd, "Internal server busy");
                LOG_ERR("%s", "Internal server busy");
                break;
            }
            timer(conn_fd, &client_addr);
        }
        return false;
    }
    return true;
}

void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust time once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

void WebServer::deal_read(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    /*reactor*/
    if (m_actor_mode == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 0);
        while (true)
        {
            /*to do...*/
        }
    }
    /*proactor*/
    else
    {

        if (users[sockfd].read_once())
        {
            LOG_INFO("deal the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            /*完成读事件，将该事件放入请求队列*/
            m_pool->append_p(users + sockfd);
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::deal_write(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    /*reactor*/
    if (m_actor_mode == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 1);
        while (true)
        {
            /*to do..*/
        }
    }
    else
    {
        /*proactor*/
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

bool WebServer::deal_signal(bool &timeout, bool &stop_server)
{
    int sig;
    char signals[1024];
    int ret = recv(m_pipe[0], signals, sizeof(signals), 0);
    if (ret < 0)
    {
        return false;
    }
    if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; i++)
        {
            switch (signals[i])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
            }
        }
    }
    return true;
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_sever = false;
    epoll_event events[MAX_EVENT_NUMBER];
    while (!stop_sever)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_INFO("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sock_fd = events[i].data.fd;
            if (sock_fd == m_listenfd)
            {
                bool flag = deal_client();
                if (!flag)
                    continue;
            }
            else if (events[i].events & EPOLLRDHUP | EPOLLHUP | EPOLLERR)
            {
                util_timer *timer = users_timer[sock_fd].timer;
                deal_timer(timer, sock_fd);
            }
            else if ((sock_fd == m_pipe[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_signal(timeout, stop_sever);
                if (!flag)
                    LOG_INFO("%s", "deal_signal failure");
            }
            else if (events[i].events & EPOLLIN)
            {
                deal_read(sock_fd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_write(sock_fd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "time tick");

            timeout = false;
        }
    }
}