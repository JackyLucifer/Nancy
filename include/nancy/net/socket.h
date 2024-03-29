#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>

#include "nancy/net/fd.h"
#include "nancy/net/details/typedef.h"

namespace nc::net {

// 套接字基类
struct socket_base {
    socket_base() = default;
    virtual ~socket_base() = default;
    virtual int get_fd() const = 0;
    virtual int release() = 0;  
    virtual void shutdown() = 0;
};

// 基于tcp协议的服务端socket
class tcp_serv_socket : public socket_base {
    int sock = 0;
public:
    explicit tcp_serv_socket() {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        assert(-1 != sock);
    }
    explicit tcp_serv_socket(int fd): sock(fd) {}
    tcp_serv_socket(const tcp_serv_socket&) = delete;
    tcp_serv_socket(tcp_serv_socket&& other) noexcept {
        sock = other.release();
    }
    tcp_serv_socket& operator=(const tcp_serv_socket&) = delete;
    tcp_serv_socket& operator=(tcp_serv_socket&& other) {
        sock = other.release();
        return *this;
    }
    ~tcp_serv_socket() {
        shutdown();
    }

public:
    /**
     * @brief 监听连接请求
     * @param ip 地址
     * @param port 端口
     */
    void listen_req(const char* ip, int port) {
        struct sockaddr_in sock_addr;
        memset(&sock_addr, 0, sizeof(sock_addr));
        inet_pton(AF_INET, ip, &sock_addr.sin_addr);
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_port = htons(port);

        if (-1 == bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr))) {
            throw std::runtime_error(std::string("Nancy-socket: ")+std::string(strerror(errno)));
        }
        if (-1 == listen(sock, 30)) {
            throw std::runtime_error(std::string("Nancy-socket: ")+std::string(strerror(errno)));
        }
    }

    /**
     * @brief 接收一个连接
     * @return 成功: fd ; 失败: -1
     */
    int accept_req() {
        struct sockaddr_in tcp_clnt_addr;
        socklen_t addr_sz = sizeof(tcp_clnt_addr);
        return accept(sock, (struct sockaddr*)&tcp_clnt_addr, &addr_sz);
    }

    /**
     * @brief 返回内部fd
     * @return fd
     */
    int get_fd() const noexcept override {
        return sock;
    }


    /**
     * @brief 关闭套接字
     * @note 该函数时可重入的，但不建议随意调用，导致性能损失
     */
    void shutdown() noexcept override {
        if (sock) {
            close(sock);
            sock = 0;
        }
    }

    /**
     * @brief 获取该fd并将内部fd置为0，类似智能指针的release
     * @return 内部fd
     */
    int release() noexcept override {
        int ret = sock;
        sock = 0;
        return ret;
    }

};

// 基于tcp协议的客户端socket，在析构函数中可以自动关闭socket
// 封装了连接远程或本地服务器的接口
class tcp_clnt_socket : public socket_base {
    int sock = 0;

public:
    explicit tcp_clnt_socket() {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        assert(-1 != sock);
    }
    explicit tcp_clnt_socket(int fd): sock(fd) {}
    tcp_clnt_socket(const tcp_clnt_socket&) = delete;
    tcp_clnt_socket(tcp_clnt_socket&& other) noexcept {
        sock = other.release();
    }
    tcp_clnt_socket& operator=(const tcp_clnt_socket&) = delete;
    tcp_clnt_socket& operator=(tcp_clnt_socket&& other) noexcept {
        sock = other.release();
        return *this;
    }

    ~tcp_clnt_socket() noexcept {
        shutdown();
    }

public:
    /**
     * @brief 发起连接请求
     * @param remote_ip 远程ip
     * @param remote_port 远程端口
     * @return 失败抛出异常信息
     */
    void launch_req(const char* remote_ip, int remote_port) {
        struct sockaddr_in sock_addr;
        memset(&sock_addr, 0, sizeof(sock_addr));
        inet_pton(AF_INET, remote_ip, &sock_addr.sin_addr);
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_port = htons(remote_port);
        if (-1 == connect(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr))) {
            std::runtime_error(std::string("Nancy-socket: ")+std::string(strerror(errno)));
        }
    }

    /**
     * @brief 返回内部fd
     * @return fd
     */
    int get_fd() const noexcept override {
        return sock;
    }
    /**
     * @brief 关闭套接字
     * @note 该函数时可重入的，但不建议随意调用，导致性能损失
     */
    void shutdown() noexcept override {
        if (sock) {
            close(sock);
            sock = 0;
        }
    }
    /**
     * @brief 获取该fd并将内部fd置为0，类似智能指针的release
     * @return 内部fd
     */
    int release() noexcept override {
        int ret = sock;
        sock = 0;
        return ret;
    }
};

/**
 * @brief a pair of socket
 * @note 全双工
*/
class sockpair {
    int fds[2] = {0};
    static const int L = 0;
    static const int R = 1;
public:
    sockpair() {
        if (-1 == socketpair(PF_UNIX, net::TCP_PROTOCOL ,0, fds)) {
            perror("Init sockpair");
            abort();
        }
    }
    sockpair(const sockpair&) = delete;
    sockpair(sockpair&& other) {
        std::swap(fds[L], other.fds[L]);
        std::swap(fds[R], other.fds[R]);
    }
    ~sockpair() {
        close(fds[L]);
        close(fds[R]);
    }
    int get_lfd() const {
        return fds[L];
    }
    int get_rfd() const {
        return fds[R];
    }
    int write_lfd(const char* buf, int sz) const {
        return write(get_lfd(), buf, sz);
    }
    int write_rfd(const char* buf, int sz) const {
        return write(get_rfd(), buf, sz);
    }
    int read_lfd(char* buf, int sz) const {
        return read(get_lfd(), buf, sz);
    }
    int read_rfd(char* buf, int sz) const {
        return read(get_rfd(), buf, sz);
    }
    int get_capacity() const {
        return net::get_send_bufsz(fds[L]);
    }
};


}  // namespace nc::net