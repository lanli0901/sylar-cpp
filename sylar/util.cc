#include "util.h"
#include "log.h"
#include <execinfo.h>  // ::backtrace 头文件

namespace sylar{

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

pid_t GetThreadID(){
    return syscall(SYS_gettid);
}

uint32_t GetFiberID()
{
    return 0;
}

void Backtrace(std::vector<std::string> &bt, int size, int skip)
{
    void** array = (void**)malloc((sizeof(void*) * size));
    // backtrace 获取当前栈的回溯信息
    // size 是最多获取的栈帧数量，s 返回实际获取的栈帧数， 并保存在 array 中。
    size_t s = ::backtrace(array, size);

    // backtrace_symbols 函数将通过 backtrace 获取的栈帧地址转换为符号信息
    // backtrace_symbols()函数可以将每一个返回值都翻译成“函数名+函数内偏移量+函数返回值”
    char** strings = backtrace_symbols(array, s);
    if(strings == NULL){
        SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error";
        return;
    }

    for(size_t i = skip; i<s; ++i){
        bt.push_back(strings[i]);
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string &prefix)
{
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(size_t i=0; i<bt.size(); ++i){
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

}