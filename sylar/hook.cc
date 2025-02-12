#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include <dlfcn.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar{

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init(){
    static bool is_inited = false;
    if(is_inited){
        return;
    }
    // define中的"##"将两个字符串链接起来
    // dlsym 是一个用于在运行时动态查找符号（例如函数或变量）地址的函数
    // 使用RTLD_NEXT参数后dlsym返回的就是第一个遇到(匹配上)name这个符号的函数的函数地址
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

struct _HookIniter
{
    _HookIniter(){
        hook_init();
    }
};

// 全局变量在main函数之前创建，因此在创建_HookIniter类变量时，会在main之前调用构造函数以此实现hook的初始化
static _HookIniter s_hook_initer;


bool is_hook_enable()
{
    return t_hook_enable;
}

void set_hook_enable(bool flag)
{
    t_hook_enable = flag;
}

}

struct timer_info
{
    // 用来标记定时器是否被取消或超时。
    int cancelled = 0;
};

// 这里的type模板中的&&不代表右值引用，而是万能引用，其既能接收左值又能接收右值。name和class用法一致
// 可变参数模板，这里的Args可以是0个或多个
// 模板中的&&不代表右值引用，而是万能引用，其既能接收左值又能接收右值。
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, 
    uint32_t event, int timeout_so, Args&&... args){
    if(!sylar::t_hook_enable){
        // std::forward<Args>(args) 的作用是确保每个参数在传递时保持其原始的值类别（左值或右值）
        return fun(fd, std::forward<Args>(args)...);
    }

    sylar::Fdctx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    // 句柄不存在
    if(!ctx){
        return fun(fd, std::forward<Args>(args)...);
    }
    // 句柄关闭
    if(ctx->isClose()){
        errno = EBADF;
        return -1;
    }
    // 句柄不是socket 或 用户设置了非阻塞
    if(!ctx->isSocket() || ctx->getUserNonblock()){
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    // 超时条件
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    // 执行一次IO操作
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    // EINTR（系统调用被信号中断）和 EAGAIN（暂时无法完成操作）
    while(n == -1 && errno == EINTR){
        n = fun(fd, std::forward<Args>(args)...);
    }
    // 做异步操作
    if(n == -1 && errno == EAGAIN){
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);     // 条件定时器的条件
        // 超时时间不等于-1(有设置超时),设置条件定时器
        if(to != (uint64_t)-1){
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                // winfo.lock() 尝试将 std::weak_ptr 转换为 std::shared_ptr。
                // weak_ptr 不拥有资源，它只是对资源的观察，因此调用 lock() 方法返回一个 shared_ptr，
                // 如果资源已经被销毁（即 winfo 指向的对象已被取消或过期），lock() 将返回一个空的 shared_ptr（即 t 为 nullptr）
                auto t = winfo.lock();
                if(!t || t->cancelled){
                    return;
                }
                // ETIMEDOUT 表示操作超时
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }
        // 如果不设置条件定时器，则默认当前协程进行io操作
        int rt = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if(rt){
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << "addEvent(" << fd << ", " << event <<")";
            if(timer){
                timer->cancel();
            }
            return -1;
        }
        else{
            // 定时器超时 或者 触发了事件 都会唤醒
            sylar::Fiber::YieldToHold();
            if(timer){
                timer->cancel();
            }
            // 如果通过定时任务唤醒
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }
            // 否则是触发事件唤醒的，就需要循环
            goto retry;
        }
    }
    return n;
}

extern "C"{
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds){
    if(!sylar::t_hook_enable){
        return sleep_f(seconds);
    }

    sylar::Fiber::ptr fiber =  sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(seconds*1000, std::bind((void(sylar::Scheduler::*)
        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
    // iom->addTimer(seconds*1000, [iom, fiber](){
    //     iom->schedule(fiber);
    // });
    sylar::Fiber::YieldToHold();
    return 0;
};

int usleep(useconds_t usec){
    if(!sylar::t_hook_enable){
            return usleep_f(usec);
        }

    sylar::Fiber::ptr fiber =  sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(usec/1000, std::bind((void(sylar::Scheduler::*)
        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
    // iom->addTimer(usec/1000, [iom, fiber](){
    //     iom->schedule(fiber);
    // });
    sylar::Fiber::YieldToHold();
    return 0;
};

int nanosleep(const struct timespec *reg, struct timespec *rem){
    if(!sylar::t_hook_enable){
        return nanosleep_f(reg, rem);
    }
    int timeout_ms = reg->tv_sec * 1000 + reg->tv_nsec/1000/1000;
    sylar::Fiber::ptr fiber =  sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
    // iom->addTimer(timeout_ms, [iom, fiber](){
    //     iom->schedule(fiber);
    // });
    sylar::Fiber::YieldToHold();
    return 0;
}

int socket(int domain, int type, int protocol){
    if(!sylar::t_hook_enable){
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1){
        return fd;
    }
    sylar::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    return connect_f(sockfd, addr, addrlen);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen){
    int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0){
        sylar::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count){
    return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt){
    return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags){
    return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen){
    return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){
    return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count){
    return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt){
    return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags){
    return do_io(sockfd, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen){
    return do_io(sockfd, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags){
    return do_io(sockfd, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd){
    if(!sylar::t_hook_enable){
        return close_f(fd);
    }

    sylar::Fdctx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(ctx){
        auto iom = sylar::IOManager::GetThis();
        if(iom){
            iom->cancelAll(fd);
        }
        sylar::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ );

int ioctl(int fd, unsigned long request, ...);

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);

}


