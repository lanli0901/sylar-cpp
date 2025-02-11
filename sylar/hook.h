#ifndef __SYLAR_HOOK_H
#define __SYLAR_HOOK_H

#include <unistd.h>

namespace sylar {
    bool is_hook_enable();
    void set_hook_enable(bool flag);
}

extern "C" {        // extern "C" 就是用来告知编译器，接下来的代码遵循 C 语言的链接约定，而不进行 C++ 的名字修饰。

typedef unsigned int (*sleep_fun)(unsigned int seconds);
extern  sleep_fun sleep_f;

typedef  int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;
}

#endif