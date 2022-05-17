/*
 * @Description:
 * @Version: 2.0
 * @Autor: Yogaguo
 * @Date: 2022-05-17 10:49:53
 * @LastEditors: Yogaguo
 * @LastEditTime: 2022-05-17 11:37:23
 */
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include <iostream>
#include <sys/time.h>
#include <pthread.h>
#include "../locker.h"
template <class T>
class block_queue
{
public:
    explicit block_queue(size_t);
    ~block_queue();
    void clear();

    bool full();

    bool empty();

    bool front(T &);

    bool back(T &);

    size_t size();

    size_t max_size();

    bool push(const T &item);

    bool pop(T &item);

    /*≥¨ ±¥¶¿Ì*/
    bool pop(T &item, size_t ms_time);

private:
    locker m_mutex;
    cond m_cond;
    T *m_array;
    size_t m_size;
    size_t m_max_size;
    size_t m_front;
    size_t m_back;
};
#endif