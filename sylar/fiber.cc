#include "fiber.h"
#include "config.h"
#include "macro.h"
#include <atomic>

namespace sylar{

static std::atomic<u_int64_t> s_fiber_id(0);
static std::atomic<u_int64_t> s_fiber_count(0);

static thread_local Fiber* t_fiber = nullptr;   // 每个线程都有一个独立的 t_fiber 变量。
static thread_local std::shared_ptr<Fiber::ptr> t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 1024*1024, "fiber stack size");

class MallocStackAllocator{
public:
    static void* Alloc(size_t size){
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t){
        return free(vp);
    }

};

// c++11引入， 等于：typedef MallocStackAllocator StackAllocator;
// using 新类型名 = 原类型名;
using StackAllocator = MallocStackAllocator;

Fiber::Fiber()
{
    m_state = EXEC;
    SetThis(this);
    // 调用 getcontext 获取当前线程的上下文信息，并保存在 m_ctx 中
    if(getcontext(&m_ctx)){
        SYLAR_ASSERT2(false, "getcontext");
    }
    ++s_fiber_count;
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize)
    :m_id(++s_fiber_id), m_cb(cb){
    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)){
        SYLAR_ASSERT2(false, "getcontext");
    }
    // 设置 uc_link 为 nullptr，表示协程结束后不会自动跳转
    m_ctx.uc_link = nullptr;
    // 设置协程栈的起始地址和大小
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    // 指定协程执行的函数（MainFunc）和参数
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
}

Fiber::~Fiber()
{
    --s_fiber_count;
    if(m_stack){
        SYLAR_ASSERT(m_state == TERM || m_state == INIT);
        StackAllocator::Dealloc(m_stack, m_stacksize);
    }
    else{
        SYLAR_ASSERT(!m_cb);
        SYLAR_ASSERT(m_state == EXEC);
        
        Fiber* cur = t_fiber;
        if(cur == this){
            SetThis(nullptr);
        }
    }
}

void Fiber::reset(std::function<void()> cb)
{
}

void Fiber::swapIn()
{
}

void Fiber::swapOut()
{
}

void Fiber::SetThis(Fiber *f)
{
}

Fiber::ptr Fiber::GetThis()
{
    return Fiber::ptr();
}

void Fiber::YieldToReady()
{
}

void Fiber::YieldToHold()
{
}

uint64_t Fiber::TotalFibers()
{
    return 0;
}

void Fiber::MainFunc()
{
}

}