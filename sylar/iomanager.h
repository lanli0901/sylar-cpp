#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace sylar{

class IOManager : public Scheduler, public TimerManager{
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event{
        NONE        = 0x0,
        READ        = 0x1,      // EPOLLIN
        WRITE       = 0x4,      // EPOLLOUT
    };

private:
    struct FdContext
    {
        typedef Mutex MutexType;
        struct EventContext
        {
            Scheduler* scheduler = nullptr;     // 事件执行的scheduler
            Fiber::ptr fiber;                   // 事件协程
            std::function<void()> cb;           // 事件的回调函数
        };

        EventContext& getcontext(Event event);      // 获取事件上下文类
        void resetContext(EventContext& ctx);       // 重置事件上下文
        void triggerEvent(Event event);             // 触发事件

        EventContext read;          // 读事件
        EventContext write;         // 写事件
        int fd = 0;                 // 事件关联的句柄
        Event events = NONE;        // 已经注册的事件
        MutexType mutex;
    };
    
public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    ~IOManager();

    // 0 success, -1 error
    // 添加事件
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    // 删除事件
    bool delEvent(int fd, Event event);
    // 取消事件
    bool cancelEvent(int fd, Event event);
    // 取消所有事件
    bool cancelAll(int fd);
    // 返回当前的IOManager
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertedAtFront() override;
    // 重置socket句柄上下文的容器大小
    void contextResize(size_t size);
    // 判断是否可以停止
    bool stopping(uint64_t& timeout);

private:
    int m_epfd = 0;         // epoll 文件句柄
    int m_tickleFds[2];     // pipe 文件句柄  1写0读

    std::atomic<size_t> m_pendingEventCount = {0};      // 当前等待执行的事件数量
    RWMutexType m_mutex;
    std::vector<FdContext*> m_fdContexts;               // socket事件上下文的容器
};


}

#endif