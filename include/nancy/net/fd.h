#pragma once

#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <iostream>

namespace nc::net {

// 设置socket为非阻塞
static inline int set_nonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * @brief 快速复用端口
 * @note 失败时抛出异常
 */
static inline void set_reuse_address(int fd) {
    assert(fd >= 0);
    int option = 1;
    socklen_t optlen = sizeof(option);
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen)) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
}

// 设置TCP发送缓冲区大小
static inline void set_send_bufsz(int sock, socklen_t size) {
    assert(sock >= 0);
    int buf_size = size;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)(&buf_size), sizeof(buf_size))) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
}

// 设置TCP接收缓冲区大小
static inline void set_recv_bufsz(int sock, socklen_t size) {
    assert(sock >= 0);
    int buf_size = size;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*)(&buf_size), sizeof(buf_size))) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
}

// 获取当前TCP发送缓冲区大小
static inline int get_send_bufsz(int sock) {
    assert(sock >= 0);
    int buf_size = 0;
    socklen_t res = sizeof(buf_size);
    if (-1 == getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, (socklen_t*)&res)) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
    return buf_size;
}

// 获取当前TCP接收缓冲区大小
static inline int get_recv_bufsz(int sock) {
    assert(sock >= 0);
    int buf_size = 0;
    socklen_t res = sizeof(buf_size);
    if (-1 == getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, (socklen_t*)&res)) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
    return buf_size;
}

// 禁止Nagle算法
static inline void set_tcp_nondelay(int sock) {
    assert(sock >= 0);
    int option;
    if (-1 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)(&option), sizeof(option))) {
        throw std::runtime_error(std::string("Nancy-fd: ")+std::string(strerror(errno)));
    }
}


}  // namespace nc::net