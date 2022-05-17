/*
 * @Description:
 * @Version: 2.0
 * @Autor: Yogaguo
 * @Date: 2022-05-17 11:03:07
 * @LastEditors: Yogaguo
 * @LastEditTime: 2022-05-17 12:37:27
 */
#include "block.queue.h"

template <class T>
block_queue<T>::block_queue(size_t max_size)
{
    if (max_size <= 0)
    {
        exit(1);
    }
    m_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template <class T>
void block_queue<T>::clear()
{
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template <class T>
block_queue<T>::~block_queue()
{
    m_mutex.lock();
    if (m_array != NULL)
        delete[] m_array;
    m_mutex.unlock;
}

template <class T>
bool block_queue<T>::full()
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <class T>
bool block_queue<T>::empty()
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <class T>
bool block_queue<T>::front(T &value)
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <class T>
bool block_queue<T>::back(T &value)
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <class T>
size_t block_queue<T>::size()
{
    size_t res = 0;
    m_mutex.lock();
    res = m_size();
    m_mutex.unlock();
    return res;
}

template <class T>
size_t block_queue<T>::max_size()
{
    size_t tmp = 0;
    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();
    return tmp;
}

template <class T>
bool block_queue<T>::push(const T &item)
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }

    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

template <class T>
bool block_queue<T>::pop(T &item)
{
    m_mutex.lock();

    /*如果用多个消费者用，while*/
    while (m_size <= 0)
    {
        if (!m_cond.wait(m_mutex.get()))
        {
            m_mutex.unlock();
            return false;
        }
    }

    m_front = (m_front + 1) % max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}
