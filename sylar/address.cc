#include "address.h"
#include "endian.h"
#include <sstream>
#include <string.h>

namespace sylar{

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

}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len)
{
    return IPAddress::ptr();
}

IPAddress::ptr IPv4Address::subnetAddress(uint32_t prefix_len)
{
    return IPAddress::ptr();
}

uint32_t IPv4Address::getPort() const
{
    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v)
{
    m_addr.sin_port = byteswapOnLittleEndian(v);
}

IPv6Address::IPv6Address()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const char *address, uint32_t port)
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
    return IPAddress::ptr();
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len)
{
    return IPAddress::ptr();
}

IPAddress::ptr IPv6Address::subnetAddress(uint32_t prefix_len)
{
    return IPAddress::ptr();
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