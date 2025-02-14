#include "address.h"
#include "log.h"
#include "endian.h"
#include <sstream>
#include <netdb.h>
#include <string.h>
#include <ifaddrs.h>

namespace sylar{

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

// 生成子网掩码的反码，bits是要保留的位数
template<class T>
static T CreateMask(uint32_t bits){
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

// 计算value中有多少个 1 （！！！巧妙！！！）
template<class T>
static uint32_t CountBytes(T value){
    uint32_t result = 0;
    for(; value; ++result){
        value &= value - 1;
    }
    return result;
}

Address::ptr Address::Create(const sockaddr *addr, socklen_t addrlen)
{
    if(addr == nullptr){
        return nullptr;
    }

    Address::ptr result;
    switch(addr->sa_family){
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknowAddress(*addr));
            break;
    }
    return result;
}

bool Address::Lookup(std::vector<Address::ptr> &result, const std::string &host, int family, int type, int protocol)
{
    addrinfo hints, *results, *next;
    hints.ai_flags = 0;             // 地址信息标志
    hints.ai_family = family;       // 地址族(AF_INET, AF_INET6, AF_UNSPEC)
    hints.ai_socktype = type;       // 套接字类型(SOCK_STREAM, SOCK_DGRAM)
    hints.ai_protocol = protocol;   // 协议号(IPPROTO_TCP, IPPROTO_UDP)，或0表示任意协议
    hints.ai_addrlen = 0;           // 地址长度
    hints.ai_canonname = NULL;      // 规范名字(主机名或服务名)
    hints.ai_addr = NULL;           // 网络地址结构指针
    hints.ai_next = NULL;           // 指向下一个addrinfo结构的指针

    std::string node;
    const char* service = NULL;

    // node获取主机名部分，service指向端口号部分。
    // 检查 ipv6address service
    if(!host.empty() && host[0] == '['){
        // memchr 在 host字符串的前host.size() - 1个字符搜索第一次出现"]"的位置, 搜索到了返回指针
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if(endipv6){
            // TODO check of range
            if(*(endipv6 + 1) == ':'){
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    // 检查 node service
    if(node.empty()){
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if(service){
            // 找到冒号且后面没有其他冒号，说明host包含了主机名和端口号。
            if(!memchr(service + 1, ':', host.size() + host.c_str() - service -1)){
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if(node.empty()){
        node = host;
    }
    // 如果getaddrinfo成功，results将包含一个或多个符合条件的地址信息
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error){
        SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddress(" << host << ", " << family <<
            ", " << type << ") err=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    next = results;
    while(next){
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }
    freeaddrinfo(results);
    return true;
}

Address::ptr Address::LookupAny(const std::string &host, int family, int type, int protocol)
{
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)){
        return result[0];
    }
    return nullptr;
}


IPAddress::ptr Address::LookupAnyIPAddress(const std::string &host, int family, int type, int protocol)
{
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)){
        for(auto& i : result){
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if(v){
                return v;
            }
        }
    }
    return nullptr;
}

bool Address::GetInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result, int family)
{
    // struct ifaddrs {
	// struct ifaddrs  *ifa_next;    // 指向列表中下一个结构的指针。该字段在列表的最后一个结构中为 NULL
	// char            *ifa_name;   // 接口名称
	// unsigned int     ifa_flags;   // 提供有关接口的一些信息的标志
	// struct sockaddr *ifa_addr;   // 接口地址
	// struct sockaddr *ifa_netmask; // 接口的网络掩码
    // ...
    // }
    struct ifaddrs *next, *results;
    if(getifaddrs(&results) != 0){
        SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress getifaddrs error! err=" << errno
            << " errstr=" << strerror(errno); 
            return false;
    }

    try{
        for(next = results; next; next = next->ifa_next){
            Address::ptr addr;
            // ~0u = 4294967295(0的反码无符号整型输出)
            uint32_t prefix_length = ~0u; 
            if(family != AF_UNSPEC && family != next->ifa_addr->sa_family){
                continue;
            }
            switch(next->ifa_addr->sa_family){
                case AF_INET:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        prefix_length = CountBytes(netmask);
                    }
                    break;
                case AF_INET6:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        prefix_length = 0;
                        for(int i = 0; i < 16; ++i){
                            prefix_length += CountBytes(netmask.s6_addr[i]);
                        }
                    }
                    break;
                default:
                    break;
            }
            if(addr){
                result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_length)));
            }
        }
    }catch(...){
        SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return true;
}

bool Address::GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>> &result, const std::string &iface, int family)
{
    if(iface.empty() || iface == "*"){
        if(family == AF_INET || family == AF_UNSPEC){
            result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
        }
        if(family == AF_INET6 || family == AF_UNSPEC){
            result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
    if(!GetInterfaceAddress(results, family)){
        return false;
    }

    // 其first和second成员都成为迭代器，且分别指向输入序列中所有值等于 val 的元素所组成的子序列的起始及末尾（即最后一个元素之后的位置）位置。
    // first 成员的值等同于 std::lower_bound 执行于同一输入序列后的返回。
    // second 成员的值等同于 std::upper_bound 执行于同一输入序列后的返回。
    auto its = results.equal_range(iface);
    for(; its.first != its.second; ++its.first){
        result.push_back(its.first->second);
    }
    return true;
}

int Address::getFamily() const
{
    return getAddr()->sa_family;
}

std::string Address::toString()
{
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address &rhs) const
{
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    // 把存储区 str1 和存储区 str2 的前 minlen 个字节进行比较。
    int result = memcmp(getAddr(), rhs.getAddr(), minlen);
    if(result < 0){
        return true;
    }
    else if(result > 0){
        return false;
    }
    else if(getAddrLen() < rhs.getAddrLen()){
        return true;
    }
    return false;
}

bool Address::operator==(const Address &rhs) const
{
    return getAddrLen() == rhs.getAddrLen() && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address &rhs) const
{
    return !(*this == rhs);
}


IPAddress::ptr IPAddress::Create(const char *address, uint32_t port)
{
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));

    hints.ai_flags = AI_NUMERICHOST;    // 地址信息标志, AI_NUMERICHOST表示提供的 address 参数是一个数字 IP 地址（即直接是 IP 地址字符串），而不是域名。
    hints.ai_family = AF_UNSPEC;        // 地址族(AF_INET, AF_INET6, AF_UNSPEC)，AF_UNSPEC 表示我们希望支持所有地址族（IPv4、IPv6）

    // getaddrinfo 是一个用于解析地址（IP 地址或主机名）到 sockaddr 结构的函数。
    // hints是提供给函数的提示信息，results是结果链表
    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error){
        SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address << ", " << port << ") error="
            << error << " errno" << errno << " errstr=" << strerror(errno);
            return nullptr;
    }

    try{
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
            // struct sockaddr *ai_addr;      // 网络地址结构指针
            // socklen_t        ai_addrlen;   // 地址长度
            Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
        if(result){
            result->setPort(port);
        }
        freeaddrinfo(results);  // freeaddrinfo 用于释放 getaddrinfo 返回的地址信息链表，防止内存泄漏。
        return result;
    }catch(...){
        freeaddrinfo(results);
        return nullptr;
    }
}


IPv4Address::ptr IPv4Address::Create(const char *address, uint32_t port)
{
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
    if(result <= 0){
        SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", " <<
            port << ") rt=" << result << " errno=" << errno << " errstr" << strerror(errno); 
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in &address)
{
    m_addr = address;
}

IPv4Address::IPv4Address(uint32_t address, uint32_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = byteswapOnLittleEndian(port);
    m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

const sockaddr *IPv4Address::getAddr() const
{
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const
{
    return sizeof(m_addr);
}

std::ostream &IPv4Address::insert(std::ostream &os) const
{
    uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "." 
        << ((addr >> 8) & 0xff) << "." << (addr & 0xff);
    os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len)
{
    if(prefix_len > 32){
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    baddr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len)
{
    if(prefix_len > 32){
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    baddr.sin_addr.s_addr &= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetAddress(uint32_t prefix_len)
{
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const
{
    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v)
{
    m_addr.sin_port = byteswapOnLittleEndian(v);
}

IPv6Address::ptr IPv6Address::Create(const char *address, uint32_t port)
{
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if(result <= 0){
        SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", " <<
            port << ") rt=" << result << " errno=" << errno << " errstr" << strerror(errno); 
        return nullptr;
    }
    return rt;
}

IPv6Address::IPv6Address()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6 &address)
{
    m_addr = address;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint32_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteswapOnLittleEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}


const sockaddr *IPv6Address::getAddr() const
{
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const
{
    return sizeof(m_addr);
}

std::ostream &IPv6Address::insert(std::ostream &os) const
{
    os << "[";
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    bool used_zeros = false;    // 用于表示是否已使用 :: 来简化地址中的连续零块
    // IPv6 地址有 8 个段，每个段是 16 位
    for(size_t i = 0; i < 8; ++i){
        // 如果当前段为零，并且还没有使用 :: 来简化零块，就跳过当前段。也就是说，连续的零块会被跳过，直到遇到非零的段。
        if(addr[i] == 0 && !used_zeros){
            continue;
        }
        // 找到连续零段后的第一个非零段，并输出“::”
        if(i && addr[i-1] == 0 && !used_zeros){
            os << ":";
            used_zeros = true;
        }
        if(i){
            os << ":";
        }
        // std::hex 让流中后续输出的整数以十六进制表示
        // std::dec 让流中后续输出的整数以十进制表示
        os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
    }

    // 如果没有使用过 ::，并且最后一个段是零，插入 ::
    if(!used_zeros && addr[7] == 0){
        os << "::";
    }
    os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len)
{
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
    for(uint32_t i = prefix_len / 8 + 1; i < 16; ++i){
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }

    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len)
{
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint8_t>(prefix_len % 8);
    // for(int i = prefix_len / 8 + 1; i < 16; ++i){
    //     baddr.sin6_addr.s6_addr[i] = 0x00;
    // }
    for(uint32_t i = 0; i < prefix_len / 8; ++i){
        baddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetAddress(uint32_t prefix_len)
{
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
    for(uint32_t i = 0 ; i < prefix_len / 8; ++i){
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const
{
    return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint32_t v)
{
    m_addr.sin6_port = byteswapOnLittleEndian(v);
}

// sun_path 108字节， 则MAX_PATH_LEN为127
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;
UnixAddress::UnixAddress()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    // offsetof(sockaddr_un, sun_path) 用来计算 sockaddr_un 结构体中 sun_path 字段相对于结构体开头的偏移量。
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string &path)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length = path.size() + 1;

    if(!path.empty() && path[0] == '\0'){
        --m_length;
    }
    if(m_length > sizeof(m_addr.sun_path)){
        throw std::logic_error("path too long");
    }

    memcpy(m_addr.sun_path, path.c_str(), m_length);
    m_length += offsetof(sockaddr_un, sun_path);

}

const sockaddr *UnixAddress::getAddr() const
{
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const
{
    return m_length;
}

std::ostream &UnixAddress::insert(std::ostream &os) const
{
    if(m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        return os << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

UnknowAddress::UnknowAddress(int family)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknowAddress::UnknowAddress(const sockaddr &addr)
{
    m_addr = addr;
}

const sockaddr *UnknowAddress::getAddr() const
{
    return &m_addr;
}

socklen_t UnknowAddress::getAddrLen() const
{
    return sizeof(m_addr);
}

std::ostream &UnknowAddress::insert(std::ostream &os) const
{
    os << "[UnknowAddress family=" << m_addr.sa_family << "]";
    return os;
}

}
