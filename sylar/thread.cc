#include "thread.h"
#include "log.h"
#include "util.h"
#include <iostream>

namespace sylar{

// 用thread_local修饰的变量具有thread周期，每一个线程都拥有并只拥有一个该变量的独立实例
static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


Thread *Thread::GetThis()
{
    return t_thread;
}

const std::string &Thread::GetName()
{
    // std::cout << "GetName() t_thread_name: " << t_thread_name << std::endl;
    return t_thread_name;
}

void Thread::SetName(const std::string &name)
{
    if(t_thread){
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

void *Thread::run(void *arg)
{
    Thread* thread = (Thread*) arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadID();
    // 设置线程的名称, name不能超过16个字节
    pthread_setname_np(pthread_self(), thread->m_name.substr(0,15).c_str());
    std::function<void()> cb;
    cb.swap(thread->m_cb);
    cb();
    return 0;
}

Thread::Thread(std::function<void()> cb, const std::string &name)
    :m_cb(cb), m_name(name)
{
    if(name.empty()){
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    // 线程创建成功，则返回0。若线程创建失败，则返回出错编号.
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
}

Thread::~Thread()
{
    if(m_thread){
        // 使用pthread_detach函数后，线程在退出后，会自己清理资源
        // 相较pthread_join,使用pthread_detach函数不会阻塞主线程，但是无法获取线程的返回值。
        pthread_detach(m_thread);
    }
}

void Thread::join()
{
    if(m_thread){
        // phtread_join函数会阻塞调用方，直至pthread_join所指的线程退出。
        int rt = pthread_join(m_thread, nullptr);
        if(rt){
            SYLAR_LOG_ERROR(g_logger) << "pthread_create join fail, rt=" << rt << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}


}