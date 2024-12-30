#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <boost/lexical_cast.hpp>
#include "log.h"
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace sylar{

class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name, const std::string& description = "")
        :m_name(name), m_description(description){
            // std::transform 用来对一段序列中的每个元素进行操作, 前两个参数是输入，第三个参数是输出，第四个参数是单目操作
            // 由于 tolower 是一个 C 函数，它可以通过全局作用域 :: 调用，确保调用的是 C 标准库中的 tolower
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
    virtual ~ConfigVarBase(){}      // 基类的析构函数必须是虚析构函数，
    // 如果基类指针指向派生类对象，而基类的析构函数不是虚拟的，那么删除这个基类指针时，只会调用基类的析构函数，
    // 而不会调用派生类的析构函数。这会导致派生类对象的资源（比如动态分配的内存）没有被释放，造成内存泄漏。

    // const std::string& 返回常量引用，避免拷贝开销
    const std::string& getName() const {return m_name;}
    const std::string& getDescription() const {return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0;
    virtual std::string getTypeName() const = 0;

protected:
    std::string m_name;
    std::string m_description;
};

// F from_type, T to_type
template<class F, class T>
class LexicalCast{
public:
    T operator()(const F& v){
        return boost::lexical_cast<T>(v);
    }
};

// 容器偏特化
// vector
template<class T>
class LexicalCast<std::string, std::vector<T> >{
public:
    std::vector<T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i=0; i<node.size(); i++){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::vector<T>, std::string>{
public:
    std::string operator()(const std::vector<T>& v){
        YAML::Node node;
        for(auto& i : v){
            // YAML::Node 类型也有push_back操作
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// list
template<class T>
class LexicalCast<std::string, std::list<T> >{
public:
    std::list<T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::list<T> vec;
        std::stringstream ss;
        for(size_t i=0; i<node.size(); i++){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::list<T>, std::string>{
public:
    std::string operator()(const std::list<T>& v){
        YAML::Node node;
        for(auto& i : v){
            // YAML::Node 类型也有push_back操作
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// set
template<class T>
class LexicalCast<std::string, std::set<T> >{
public:
    std::set<T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::set<T> vec;
        std::stringstream ss;
        for(size_t i=0; i<node.size(); i++){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::set<T>, std::string>{
public:
    std::string operator()(const std::set<T>& v){
        YAML::Node node;
        for(auto& i : v){
            // YAML::Node 类型也有push_back操作
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// unordered_set
template<class T>
class LexicalCast<std::string, std::unordered_set<T> >{
public:
    std::unordered_set<T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::unordered_set<T> vec;
        std::stringstream ss;
        for(size_t i=0; i<node.size(); i++){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string>{
public:
    std::string operator()(const std::unordered_set<T>& v){
        YAML::Node node;
        for(auto& i : v){
            // YAML::Node 类型也有push_back操作
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// map
template<class T>
class LexicalCast<std::string, std::map<std::string, T> >{
public:
    std::map<std::string, T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string>{
public:
    std::string operator()(const std::map<std::string, T>& v){
        YAML::Node node;
        for(auto& i : v){
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// unordered_map
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T> >{
public:
    std::unordered_map<std::string, T> operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);        // 将字符串直接转为Node
        typename std::unordered_map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it){
            ss.str("");     // 将ss中的数据用 “” 覆盖，即字符流清空
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string>{
public:
    std::string operator()(const std::unordered_map<std::string, T>& v){
        YAML::Node node;
        for(auto& i : v){
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


// FromStr T operator()(const std::string&)
// ToStr std::string operator()(const T&)
template<class T ,class FromStr = LexicalCast<std::string, T> , 
                    class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase{
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    // std::function 是 C++11 引入的一个模板类，它位于 <functional> 头文件中，
    // 用来封装任何可调用的目标（如普通函数、成员函数、函数对象、lambda 表达式等）。
    // 它提供了一个统一的接口，使得可以将这些不同的可调用对象作为参数传递或存储。
    typedef std::function<void (const T& old_value, const T& new_value)> on_change_cb;

    ConfigVar (const std::string& name, const T& default_value, const std::string& description = "")
        : ConfigVarBase(name, description), m_val(default_value) {}
    
    std::string toString() override{
        try{
            // return boost::lexical_cast<std::string>(m_val);  // boost::lexical_cast 是类型转换工具
            return ToStr()(m_val);
        }catch(std::exception& e){
            // e.what() 返回发生异常时的错误信息
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception " << e.what() <<
            " convert: " << typeid(m_val).name() << " to string";
            // typeid(m_val).name() 用来获取 m_val 的类型信息。typeid 返回一个 std::type_info 对象，name() 方法返回类型的字符串表示。
        }
        return "";
    }

    bool fromString(const std::string& val) override{
        try{
            // m_val = boost::lexical_cast<T>(val);
            // std::cout << "try fromString" << std::endl;
            setValue(FromStr()(val));
        }catch(std::exception& e){
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromtring exception " << e.what() <<
            " convert: string to " << typeid(m_val).name() << " - " << val;
        }
        return false;
    }

    const T getValue() const { return m_val;}
    void setValue(const T& v) { 
        // m_val = v;
        if(v == m_val){
            return;
        }
        for(auto& i : m_cbs){
            i.second(m_val, v);
        }
        m_val = v;
    }

    std::string getTypeName() const override { return typeid(T).name();}

    void addListener(uint64_t key, on_change_cb cb){
        m_cbs[key] = cb;
    }

    void delListener(uint64_t key){
        m_cbs.erase(key);
    }

    on_change_cb getListener(uint64_t key){
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    void clearListener() {
        m_cbs.clear();
    }
private:
    T m_val;
    // function函数没有比较函数，所以用map封装
    // 变更回调函数组, uint64_t key要求唯一，一般可以用hash
    std::map<uint64_t, on_change_cb> m_cbs;
};

class Config{
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    template<class T>
    // typename 关键字在这里的作用是明确声明 ConfigVar<T>::ptr 是一个类型，而不是一个变量、函数或其他对象。
    static typename ConfigVar<T>::ptr Lookup(const std::string& name, 
        const T& default_value,  const std::string& description = ""){
        // auto tmp = Lookup<T>(name);
        // if(tmp){
        //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name= " << name << " exists";
        //     return tmp;
        // }     
        auto it = GetDatas().find(name);
        if(it != GetDatas().end()){
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>> (it->second);
            if(tmp){
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name= " << name << " exists";
                return tmp;
            }
            else{
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name= " << name << " exists but type not"
                    << typeid(T).name() << " real_type=" << it->second->getTypeName() << " " 
                    << it->second->toString();
                return tmp;
            }
        }
        // find_last_not_of() 返回的是字符串中最后一个不在给定字符集中的字符的位置，如果没有找到，则返回 std::string::npos
        if(name.find_last_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") 
                != std::string::npos){
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }                                    
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetDatas()[name] = v;
        return v;
    }

    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        auto it = GetDatas().find(name);
        if(it == GetDatas().end()) {
            return nullptr;
        }
        // dynamic_pointer_cast 是 std::shared_ptr 提供的安全类型转换方法，用于将基类指针转换为派生类指针，如果类型不匹配，会返回 nullptr
        return std::dynamic_pointer_cast<ConfigVar<T>> (it->second);
    }

    static void LoadFromYaml(const YAML::Node& root);

    static ConfigVarBase::ptr LookupBase(const std::string& name);

private:
    static ConfigVarMap& GetDatas(){
        static ConfigVarMap s_datas;
        return s_datas;
    }
};


}

#endif