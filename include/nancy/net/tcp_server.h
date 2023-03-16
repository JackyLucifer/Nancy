#pragma once
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>

#include "nancy/net/reactor.h"

namespace nc::net {

// =========================  tcp_server ===========================
// Base:
//   epoll
//
// surppot:
//   ip protocol: IPv4
//
// React Pattern:
//   (We call a fixed combination "ET+oneshot+nonblock" as FIXED)
//   1. serve socket: unconsidered(et+nonblock)
//   2. connect socket default: FIXED + event_readable
//   3. connect socket by add_socket():  FIXED + event?(depends on you)
//   4. modified socket: FIXED + event?(depends on you)
//   5. add signal by add_signal(): unconsidered
//   6. add signal by add_socket(): the same with the "3"
//
// else:
//   .....
// =========================  tcp_server =========================


class tcp_server {
    using callback_t = typename std::function<void(int)>;

private:
    bool stop = false;
    int epoll_fd = 0;
    int signal_fd = 0;
    net::tcp_serv_socket serv_sock;

private:
    std::unique_ptr<epoll_event[]> events = {nullptr};
    std::map<int, std::function<void()>> signal_cbs = {};

    callback_t readable_cb = {};
    callback_t writable_cb = {};
    callback_t disconnect_cb = {};
    callback_t connect_cb = {};

public:
    explicit tcp_server(const char* ip = localhost, int port = details::NET_DEFAULT_PORT) noexcept {
        // socket
        set_nonblocking(serv_sock.get_fd());
        set_reuse_address(serv_sock.get_fd());
        serv_sock.listen_req(ip, port);

        // epoll
        assert((epoll_fd = epoll_create(details::EPOLL_CREATE_SIZE)) != -1);
        events.reset(new epoll_event[details::EPOLL_EVENT_BUF_SIZE]);
        epoll_add(serv_sock.get_fd(), event::readable, pattern::et);  // ET

        // 信号
        signal_fd = signal_socket_init();
        epoll_add(signal_fd, event::readable, pattern::lt);  // LT
    }

    ~tcp_server() noexcept {
        shutdown();
    }

private:
    // 默认: disconnect)
    void epoll_add(int sock, net::event_t ev, net::pattern_t pattern) {
        static struct epoll_event event;
        event.data.fd = sock;
        event.events = ev | pattern | event::disconnect;
        if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event)) {
            throw std::runtime_error(std::string("Nancy-tcp_server: ") + strerror(errno));
        }
    }

    // 默认: event+disconnect + et_oneshot
    void epoll_mod(int sock, net::event_t ev, net::pattern_t pattern = net::pattern::et_oneshot) {
        static struct epoll_event event;
        event.data.fd = sock;
        event.events = ev | pattern::et_oneshot | event::disconnect;
        if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock, &event)) {
            throw std::runtime_error(std::string("Nancy-tcp_server: ") + strerror(errno));
        }
    }

    // 处理连接
    void deal_conn() {
        static int fd = 0;
        while (1) {
            fd = serv_sock.accept_req();
            if (fd > 0) {
                if (static_cast<bool>(connect_cb)) {
                    connect_cb(fd);
                } else {
                    set_nonblocking(fd);                          // 设置为非阻塞
                    epoll_add(fd, event::readable, pattern::et_oneshot);  // et+oneshot
                }
            } else {
                break;
            }
        }
    }

    // 处理信号
    void deal_signal() {
        int ret = 0;
        static const int buf_sz = 24;
        static char signals[buf_sz];
        ret = read(signal_fd, signals, buf_sz);
        for (int i = 0; i < ret; ++i) {
            signal_cbs[signals[i]]();  //执行回调
        }
    }

public:
    /**
     * @brief 更新套接字状态（只允许在oneshot模式下使用）
     * @param sock  套接字
     * @param state 套接字状态
     */
    void add_event(int for_whom, net::event_t event) {
        epoll_mod(for_whom, event);
    }

    /**
     * @brief 监听socket
     * @param sock
     * @param ev 读写事件
     */
    void add_socket(int sock, net::event_t ev) {
        set_nonblocking(sock);
        epoll_add(sock, ev, pattern::et_oneshot);
    }

    /**
     * @brief 添加信号
     * @tparam F 可执行回调
     * @param sig 信号类型
     * @param cb 回调
     */
    template <typename F>
    void add_signal(int sig, F&& cb) {
        signal_cbs[sig] = std::forward<F>(cb);
        signal_add(sig);
    }

    /**
     * @brief 获取监听端口的socket(fd)
     * @return serve socket
     */
    int get_server_fd() {
        return serv_sock.get_fd();
    }

    // 设置收到连接申请的回调
    template <typename F>
    void set_connect_cb(F&& cb) {
        connect_cb = std::forward<F>(cb);
    }

    // 设置可读socket回调
    template <typename F>
    void set_readable_cb(F&& cb) {
        readable_cb = std::forward<F>(cb);
    }

    // 设置可写socket回调
    template <typename F>
    void set_writable_cb(F&& cb) {
        writable_cb = std::forward<F>(cb);
    }

    // 对端关闭/异常的回调
    template <typename F>
    void set_disconnect_cb(F&& cb) {
        disconnect_cb = std::forward<F>(cb);
    }

public:
    // 进入循环阻塞监听
    void activate() {
        int fd = 0;
        int count = 0;
        int event_nums = 0;
        int serv_fd = serv_sock.get_fd();
        int curr_bufsz = details::EPOLL_EVENT_BUF_SIZE;

        while (!stop) {
            // 等待事件
            event_nums = epoll_wait(epoll_fd, events.get(), curr_bufsz, -1);
            for (int i = 0; i < event_nums; i++) {
                fd = events[i].data.fd;
                if (fd == serv_fd) {
                    deal_conn();
                } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    if (static_cast<bool>(disconnect_cb)) {
                        disconnect_cb(fd);
                    } else {
                        close(fd);
                    }
                } else if (fd == signal_fd && (events[i].events & EPOLLIN)) {
                    deal_signal();
                } else if (events[i].events & EPOLLIN) {
                    readable_cb(fd);
                } else if (events[i].events & EPOLLOUT) {
                    writable_cb(fd);
                }
            }
            // 动态调整epoll event buf
            if (event_nums == curr_bufsz) {
                curr_bufsz *= 2;
                count = 0;
            } else if (event_nums < (curr_bufsz / 2)) {
                if (++count > 128) {
                    curr_bufsz /= 2;
                    count = 0;
                } else {
                    continue;
                }
            } else {
                count = 0;
                continue;
            }
            events.reset(new epoll_event[curr_bufsz]);
        }
    }

    // 关闭监听器
    void shutdown() {
        if (!stop) {
            stop = true;
            serv_sock.shutdown();
            close(epoll_fd);
            signal_socket_close();
        }
    }
};


}  // namespace nc