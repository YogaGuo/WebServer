#ifndef _THREAD_POOL__H
#define _THREAD_POOL__H
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template <typename T>
class thread_pool
{
public:
    thread_pool(int actor_model, connection_pool *connPool,
                int thread_number = 8, int max_request = 1000);
    ~thread_pool();

    /*往请求队列添加任务*/
    bool append(T *request, int);

    bool append_p(T *request);

private:
    /*线程池中的线程数*/
    int m_thread_number;
    /*请求队列中允许的最大请求数*/
    int m_max_request;
    /*描述线程池的数组， 其大小为m_thread_number*/
    pthread_t *m_threads;

    /*请求队列*/
    std::list<T *> m_workqueue;
    /*保护请求队列的互斥锁*/
    locker m_queuelocker;

    /*是否有任务需要处理*/
    sem m_queuestat;
    /*是否结束线程*/
    bool m_stop;
    /*数据库连接池*/
    connection_pool *m_connPool;

private:
    /*工作线程函数*/
    static void *work(void *arg);
    void run();
};

template <typename T>
thread_pool<T>::thread_pool(int actor_model, connection_pool *connPool, int thread_number, int max_request)
    : m_thread_number(thread_number), m_max_request(max_request), m_stop(false), m_threads(nullptr)
{
    if ((thread_number <= 0) || (max_request <= 0))
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    for (int i = 0; i < thread_number; i++)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, NULL, work, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
thread_pool<T>::~thread_pool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool thread_pool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/*传入的是fd*/
template <typename T>
bool thread_pool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *thread_pool<T>::work(void *arg)
{
    thread_pool<T> *pool = (thread_pool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void thread_pool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();
        /*被唤醒的线程先加锁*/
        m_queuelocker.lock();
        if (!m_workqueue.size())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        request->mysql = m_connPool->GetConnection();
        request->process();

        m_connPool->ReleaseConnection(request->mysql);
    }
}
#endif