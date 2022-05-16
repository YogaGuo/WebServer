#ifndef HTTP_CONN_H
#define HTTP_CONN_H
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <map>
#include <sys/uio.h>
#include <syslog.h>
#include <sys/stat.h>
#include "../locker.h"

class http_conn
{
public:
    /*���ö�ȡ�ļ�����m_real_file��С*/
    static const int FILE_NAME_LEN = 200;
    /*���ö���������С*/
    static const int READ_BUF_SIZE = 2048;
    /**/
    static const int WRITE_BUF_SIZE = 1024;
    /*���ĵ�����ʽ�� ����ֻ�� Get POST*/
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE
    };
    /*��״̬��״̬*/
    enum CHECK_STATE
    {
        CHECH_STATE_REQUESTLINE = 0,
        CHECH_STATE_HEADER,
        CHECH_STATE_CONTENT
    };
    /*��״̬��״̬*/
    enum LINE_STATUE
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    /*���Ľ����Ľ��*/
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST, //������Դ����������Ӧ
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // ��ʼ��socket��ַ��
    void init(int sockfd, const sockaddr_in &addr, char *, int, int,
              const std::string &user, const std::string &passwd, const std::string &sqlname);

    void close_conn(bool real_closer = true);
    void process();
    /*��ȡ�����������ȫ������*/
    bool read_once();
    /*��Ӧ����д��*/
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

    /*���º�������process_read()����*/
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_header(char *text);
    HTTP_CODE parse_content(char *text);
    char *get_line();
    HTTP_CODE do_request();
    LINE_STATUE parse_line();

    /*���º�������process_write()����*/
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

    int timer_flag;
    int improv;
    int m_state; //��Ϊ0, дΪ1
private:
    int m_sockfd;
    struct sockaddr_in m_address;
    /*��������*/
    char m_read_buf[READ_BUF_SIZE];

    /*m_read_buf���һ���ֽڵ���һ��λ��*/
    int m_read_next_idx;

    int m_check_idx;

    int m_start_line;

    char m_write_buf[WRITE_BUF_SIZE];
    /*ָʾbuf�еĳ���*/
    int m_write_size;

    /*��״̬����ǰ״̬*/
    CHECK_STATE m_check_state;
    /*����ʽ*/
    METHOD m_method;

    char m_real_file[FILE_NAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    /*�ͻ������Ŀ���ļ���mmapӳ�������ʼλ��*/
    char *m_file_address;

    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;

    int cgi;
    char *m_string;      //�洢����ͷ����
    int bytes_to_send;   //ʣ�෢���ֽ���
    int bytes_done_send; //�ѷ����ֽ���

    std::map<std::string, std::string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};
#endif