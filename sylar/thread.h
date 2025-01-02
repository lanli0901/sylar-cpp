#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>

// pthread_xxx
// std::thread, pthread
namespace sylar{

class Thread{
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    pid_t getId() const { return m_id;}
    const std::string& getName() const { return m_name;}

    void join();        // 等待线程执行完成
    
    static Thread* GetThis();       // 获取当前的线程指针
    static const std::string& GetName();
    static void SetName(const std::string& name);

private:
    // delete关键字，禁止赋值和拷贝
    Thread(const Thread&) = delete;
    Thread(const Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;

    static void* run(void* arg);
private:
    pid_t m_id = -1;     // 线程id
    pthread_t m_thread = 0;     // 线程结构
    std::function<void()> m_cb;     // 线程执行函数
    std::string m_name;

};

}


#endif