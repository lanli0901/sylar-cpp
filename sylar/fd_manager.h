#ifndef __SYLAR_FD_MANAGER_H__
#define __SYLAR_FD_MANAGER_H__

#include <memory>
#include <vector>
#include "thread.h"
#include "iomanager.h"
#include "singleton.h"

namespace sylar{

class Fdctx : public std::enable_shared_from_this<Fdctx>{
public:
    typedef std::shared_ptr<Fdctx> ptr;
    Fdctx(int fd);
    ~Fdctx();

    bool init();
    bool isInit() const { return m_isInit;}
    bool isSocket() const { return m_isSocket;}
    bool isClose() const { return m_isClosed;}
    bool close();

    void setSysNonblock(bool v) { m_sysNonblock = v;}
    bool getSysNonblock() const { return m_sysNonblock;}\

    void setUserNonblock(bool v) { m_userNonblock = v;}
    bool getUserNonblock() const { return m_userNonblock;}

    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);

private:
    // 冒号后的数字 1 (位域) 指定了 m_isInit 成员仅占用 1 位 的内存空间，而不是通常的 1 字节（8 位）
    bool m_isInit: 1;       // 是否初始化
    bool m_isSocket: 1;     // 是否socket
    bool m_sysNonblock: 1;  // 是否hook非阻塞
    bool m_userNonblock: 1; // 是否用户主动设置非阻塞
    bool m_isClosed: 1;     // 是否关闭
    int m_fd;
    uint64_t m_recvTimeout;     // 读超时时间毫秒
    uint64_t m_sendTimeout;     // 写超时时间毫秒
};

// 文件句柄管理类
class FdManager{
public:
    typedef RWMutex RWMutexType;
    FdManager();

    Fdctx::ptr get(int fd, bool auto_create = false);      // 获取/创建文件句柄类FdCtx
    void del(int fd);       // 删除文件句柄类

private:
    RWMutexType m_mutex;
    std::vector<Fdctx::ptr> m_datas;      // 文件句柄集合
};

typedef Singleton<FdManager> FdMgr;

}

#endif