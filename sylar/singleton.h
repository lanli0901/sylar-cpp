#ifndef __SYLAR_SINGLETON_H__
#define __SYLAR_SINGLETON_H__

namespace sylar{

template<class T, class X = void, int N = 0>
class Singleton
{
public:
    // 获取单实例对象
    static T* GetInstance(){
        static T v;
        return &v;
    }
};

template<class T, class X = void, int N = 0>
class SingletonPtr{
public:
    // 获取单实例对象
    static std::shared_ptr<T> GetInstance(){
        static std::shared_ptr<T> v(new T);
        return &v;
    }
};

}

#endif