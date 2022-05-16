#include "http_conn.h"
#include <mysql/mysql.h>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

bool http_conn::read_once()
{
    if (m_read_next_idx >= READ_BUF_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_next_idx, READ_BUF_SIZE - m_read_next_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_next_idx += bytes_read;
    }
    return true;
}

int setnonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_flag | O_NONBLOCK);
    return old_flag;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event events;
    events.data.fd = fd;
    if (TRIGMode == 1)
        events.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        events.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot)
    {
        events.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &events);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event events;
    events.data.fd = fd;
    if (TRIGMode == 1)
        events.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        events.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events);
}

void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_done_send = 0;
    m_check_state = CHECH_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_next_idx = 0;
    m_write_size = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUF_SIZE);
    memset(m_write_buf, '\0', WRITE_BUF_SIZE);
    memset(m_real_file, '\0', FILE_NAME_LEN);
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, const std::string &user, const std::string &passwd,
                     const std::string &sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

http_conn::LINE_STATUE http_conn::parse_line()
{
    char temp;
    for (; m_check_idx < m_read_next_idx; m_check_idx++)
    {
        temp = m_read_buf[m_check_idx];
        if (temp == '\r')
        {
            if ((m_check_idx + 1) == m_read_next_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_check_idx + 1] == '\n')
            {
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_check_idx > 1 && m_read_buf[m_check_idx - 1] == '\r')
            {
                m_read_buf[m_check_idx - 1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

char *http_conn::get_line()
{
    return m_read_buf + m_start_line;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUE line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECH_STATE_CONTENT && line_status == LINE_OK) ||
           (line_status == parse_line()) == LINE_OK)
    {
        text = get_line();

        m_start_line = m_check_idx;

        switch (m_check_state)
        {

        case CHECH_STATE_REQUESTLINE:
        {
            //����������
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECH_STATE_HEADER:
        {
            /*������Ϣͷ*/
            ret = parse_header(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECH_STATE_CONTENT:
        {
            //������Ϣ�壺
            ret = parse_content(text);

            /*����post������ת��������Ӧ����*/
            if (ret == GET_REQUEST)
                return do_request();
            /*���� ����ѭ���� ���������Ϣ��*/
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
/*����http �����У�������󷽷��� Ŀ��url,http�汾��*/
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");

    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    /*��ulrl Ϊ / ʱ�� ��ʾ��ӭ����*/
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    /*��״̬��ת��*/
    m_check_state = CHECH_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_header(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECH_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strncasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atoi(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop! unkown header: %s", text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_next_idx >= (m_content_length + m_check_idx))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

const char *doc_root = "/home/Linux_server_programing/WebServer";
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    const char *p = strrchr(m_url, '/');

    /*����cgi ʵ�ֵ�¼ע��У��*/
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        //���ݱ�־�ж��ǵ�¼��⻹��ע����
        //ͬ���̵߳�¼У��
        // CGI����̵�¼У��
    }
    /*��תע�����*/
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");

        /*����վĿ¼��register.html����ƴ�ӣ� ���µ�m_real_file��*/
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    /*��ʾ��ת��¼����*/
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/login.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    /*��ӭҳ��*/
    else
    {
        strncpy(m_real_file + len, m_url, FILE_NAME_LEN - len - 1);
    }

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_size >= WRITE_BUF_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_size, WRITE_BUF_SIZE - m_write_size,
                        format, arg_list);
    if (len >= (WRITE_BUF_SIZE - m_write_size - 1))
        va_end(arg_list);
    return false;

    m_write_size += len;
    va_end(arg_list);

    return true;
}

/*���״̬��*/
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

/*�����Ϣ��ͷ�� ����Ϊ�� �ı����ȣ� ����״̬�� ����*/
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : close);
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
/**/
bool http_conn::add_content_type()
{
    return add_response("Content_Type: %s\r\n", "text/html");
}

bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
        /*�ڲ�����*/
    case INTERNAL_ERROR:
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
        /*�����﷨���� 404*/
    case BAD_REQUEST:
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    /*��Դû�з���Ȩ�� 403*/
    case FORBIDDEN_REQUEST:
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (add_content(error_403_form))
            return false;
        break;
    /*�ļ����ڣ� 200*/
    case FILE_REQUEST:
        add_status_line(200, ok_200_title);

        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_size;

            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            /*���͵�ȫ������Ϊ��Ӧ����ͷ����Ϣ �� �ļ���С*/
            bytes_to_send = m_write_size + m_file_stat.st_size;
            return true;
        }
        else
        {
            /*������Դ��СΪ0 ���ؿհ�html�ļ�*/
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    default:
        return false;
        break;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_size;
    m_iv_count = 1;
    return true;
}

bool http_conn::write()
{
    int temp = 0;
    int newaddr = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }
    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp > 0)
        {
            /*�����ѷ����ֽ�*/
            bytes_done_send += temp;
            /*ƫ���ļ�iovecָ��*/
            newaddr = bytes_done_send - m_write_size;
        }
        if (temp <= -1)
        {
            /*�жϻ������Ƿ�����*/
            if (errno == EAGAIN)
            {
                /*��һ��iovecͷ����Ϣ�����ѽ����� ���͵ڶ���iovec����*/
                if (bytes_done_send >= m_iv[0].iov_len)
                {
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + newaddr;
                    m_iv[1].iov_len = bytes_to_send;
                }
                /*�������͵�һ��iovecͷ����Ϣ*/
                else
                {
                    m_iv[0].iov_base = m_write_buf + bytes_done_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_done_send;
                }
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            /*�������ʧ�� �����ǻ����������� ȡ��ӳ��*/
            umap();
            return false;
        }

        bytes_to_send -= temp;

        /*���������Ѿ�ȫ���������*/
        if (!bytes_to_send <= 0)
        {
            umap();
            /*����epolloneshot*/
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            /*���������Ϊ������*/
            if (m_linger)
            {
                /*���³�ʼ��http����*/
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}