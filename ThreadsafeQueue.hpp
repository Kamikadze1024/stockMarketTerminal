#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

/*
 * Простая потокобезопасная очередь
 * с блокировками
 * Кашицын Денис, 2020
 */

#include <mutex>
#include <queue>
#include <condition_variable>

template<typename T>
class ThreadsafeQueue {
private:
    mutable std::mutex      m_mut;
    std::queue<T>           m_dataQueue;
    std::condition_variable m_dataCond;

public:
    ThreadsafeQueue() { }

    //опустить в очередь
    void push(T value) {
        std::lock_guard<std::mutex> lk(m_mut);
        m_dataQueue.push(std::move(value));
        m_dataCond.notify_all();
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(m_mut);
        m_dataCond.wait(lk, [this] {return !m_dataQueue.empty();});
        value = std::move(m_dataQueue.front());
        m_dataQueue.pop();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(m_mut);
        m_dataCond.wait(lk, [this] {return !m_dataQueue.empty();});
        std::shared_ptr<T> res(
                    std::make_shared<T>(std::move(m_dataQueue.front())));
        m_dataQueue.pop();
        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(m_mut);
        if(m_dataQueue.empty()) { return false; }
        value = std::move(m_dataQueue.front());
        m_dataQueue.pop();
        return true;
    }


    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(m_mut);
        if(m_dataQueue.empty()) { return std::shared_ptr<T>(); }
        std::shared_ptr<T> res(
                    std::make_shared<T>(std::move(m_dataQueue.front())));
        m_dataQueue.pop();
        return res;
    }

    //пусто?
    bool empty() const {
        std::lock_guard<std::mutex> lk(m_mut);
        return m_dataQueue.empty();
    }
};

#endif // THREADSAFEQUEUE_HPP
