#ifndef HHTP_CONN_H
#define HHTP_CONN_H
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include "../locker.h"
class http_conn
{
public:
    /*设置读取文件名称m_real_file大小*/
    static const int FILE_NAME_LEN = 200;
    /*设置读缓冲区大小*/
    static const int READ_BUF_SIZE = 2048;
    /**/
    static const int WRITE_BUF_SIZE = 1024;
    /*报文的请求方式， 我们只用 Get POST*/
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE
    };
    /*主状态机状态*/
    enum CHECK_STATE
    {
        CHECH_STATE_REQUESTLINE = 0,
        CHECH_STATE_HEADER,
        CHECH_STATE_CONTENT
    };
    /*从状态机状态*/
    enum LINE_STATUE
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    /*报文解析的结果*/
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // 初始化socket地址，
    void init(int sockfd, const struct sockaddr_in &addr);
    void close_conn(bool real_closer = true);
    void process();
    /*读取浏览器发来的全部数据*/
    bool real_once();
    /*响应报文写入*/
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result();
    void initresultFile(connection_pool *connPool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    /*以下函数都由process_read()调用*/
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_header(char *text);
    HTTP_CODE parse_content(char *text);
    char *get_line()
    {
        return m_read_buf + m_start_count;
    }
    HTTP_CODE do_request();
    LINE_STATUE parse_line();

    /*以下函数皆有process_write()调用*/
    void umap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;

private:
    int m_sockfd;
    struct sockaddr_in m_address;
    /*读缓冲区*/
    char m_read_buf[READ_BUF_SIZE];

    /*m_read_buf最后一个字节的下一个位置*/
    int m_read_next_idx;

    int m_check_idx;

    int m_start_line;

    char m_write_buf[WRITE_BUF_SIZE];
    /*待发送字节数*/
    int m_write_size;

    /*主状态机当前状态*/
    CHECK_STATE m_check_state;
    /*请求方式*/
    METHOD m_method;

    char m_real_file[FILE_NAME_LEN];
    char *m_url;
    char *m_version;
    char *host;
    int m_content_length;
    bool m_linger;

    /*客户请求的目标文件被mmap映射的其起始位置*/
    char *m_file_address;

    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;

    int cgi;
    char *m_string;      //存储请求头数据
    int bytes_to_send;   //剩余发送字节数
    int bytes_done_send; //已发送字节数
}
#endif