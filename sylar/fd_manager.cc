#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sylar{
Fdctx::Fdctx(int fd)
    :m_isInit(false), m_isSocket(false), m_sysNonblock(false), m_userNonblock(false), 
    m_isClosed(false), m_fd(false), m_recvTimeout(-1), m_sendTimeout(-1)
{
    init();
}

Fdctx::~Fdctx()
{
}

bool Fdctx::init()
{
    if(m_isInit){
        return true;
    }
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    struct stat fd_stat;            // struct stat定义了有关文件的信息，包括文件的类型、大小、权限、最后访问时间等。
    // fstat() 用来获取与给定文件描述符相关的文件信息。 返回 -1 表示发生了错误（如文件描述符无效）。
    if(-1 == fstat(m_fd, &fd_stat)){   
        m_isInit = false;
        m_isSocket = false;
    }
    else{
        m_isInit = true;
        // fd_stat.st_mode 存储了文件的类型和权限信息。S_ISSOCK() 会根据文件的模式位判断该文件是否是一个socket。
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    if(m_isSocket){
        // socket设置为非阻塞
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if(!(flags & O_NONBLOCK)){
            fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonblock = true;
    }
    else{
        m_sysNonblock = false;
    }
    
    m_userNonblock = false;
    m_isClosed = false;
    return m_isInit;
}

void Fdctx::setTimeout(int type, uint64_t v)
{   
    // SO_RCVTIMEO用来设置socket接收数据的超时时间
    if(type == SO_RCVTIMEO){
        m_recvTimeout = v;
    }
    else{
        m_sendTimeout = v;
    }
}

uint64_t Fdctx::getTimeout(int type)
{
    if(type == SO_RCVTIMEO){
        return m_recvTimeout;
    }
    else{
        return m_sendTimeout;
    }
}

FdManager::FdManager()
{
    m_datas.resize(64);
}

Fdctx::ptr FdManager::get(int fd, bool auto_create)
{
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_datas.size() <= fd){
        if(auto_create == false){
            return nullptr;
        }
    }
    else{
        // 当前fd存在 或 不需要自动创建时(nullptr) 直接返回
        if(m_datas[fd] || !auto_create){
            return m_datas[fd];
        }
    }
    lock.unlock();

    RWMutexType::WriteLock lock2(m_mutex);
    Fdctx::ptr ctx(new Fdctx(fd));
    m_datas[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd)
{
    RWMutex::WriteLock lock(m_mutex);
    if((int)m_datas.size() <= fd){
        return;
    }
    m_datas[fd].reset();
}
}