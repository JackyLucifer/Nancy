#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>
#include <functional>

#include "nancy/net/details/config.h"
#include "nancy/net/details/signal.h"
#include "nancy/net/details/typedef.h"
#include "nancy/net/socket.h"

namespace nc::net {  

// =========================  reactor ===========================
// Base:
// 	 epoll
//
// React Event+Pattern:
//   1. connect socket by add_socket(): just "disconnect"
//   2. modified socket: just "disconnect"
//   3. add signal by add_signal(): unconsidered
//   4. add signal by add_socket(): the same with the "1"
//
// Note:
//   Adding too many serv_sock/clnt_sock can seriously affect performance.
//   The reason is that we use std::map to save them , so we have to search for the callbacks each time
// =========================  reactor ===========================




// 可定制的反应堆
class reactor {
    using callback_t = typename std::function<void(int)>;

private:
    bool stop = false;
    int epoll_fd = 0;
    int signal_fd = 0;

    std::unique_ptr<epoll_event[]> events = {nullptr};
    callback_t readable_cb = {};
    callback_t writable_cb = {};
    callback_t disconnect_cb = {};
    std::unordered_map<int, std::function<void()>> cb_list = {};
    std::map<int, std::function<void()>> signal_cbs = {};
    std::vector<std::unique_ptr<net::socket_base*>> hosting;

public:
    explicit reactor() {
        assert((epoll_fd = epoll_create(details::EPOLL_CREATE_SIZE)) != -1);
        events.reset(new epoll_event[details::EPOLL_EVENT_BUF_SIZE]);

    }
    ~reactor() noexcept {
        shutdown();
    }

private:
    void epoll_add(int sock, event_t ev, pattern_t pattern) {
        struct epoll_event event;
        event.data.fd = sock;
        event.events = ev | pattern | event::disconnect;
        if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event)) {
            throw std::runtime_error(std::string("Nancy-reactor: ")+strerror(errno));
        }
    }

    void epoll_mod(int sock, event_t ev, pattern_t pattern) {
        struct epoll_event event;
        event.data.fd = sock;
        event.events = ev | pattern | event::disconnect;
        if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock, &event)) {
            throw std::runtime_error(std::string("Nancy-reactor: ")+strerror(errno));
        }
    }

    void deal_signal() {
        int ret = 0;
        const int buf_sz = 24;
        char signals[buf_sz];
        ret = read(signal_fd, signals, buf_sz);
        for (int i = 0; i < ret; ++i) {
            signal_cbs[signals[i]]();
        }
    }

public:
    // 注册信号
    template <typename F>
    void add_signal(int sig, F&& cb) {
        int fd = signal_socket_init();
        if (fd == -1) {
            throw std::logic_error("Nancy-reactor: init signal handle twice");
        } else {
            signal_fd = fd;
            epoll_add(signal_fd, event::readable, pattern::lt);
        }

        signal_cbs[sig] = std::forward<F>(cb);
        signal_add(sig);
    }

    /**
     * @brief 将套接字添加到反应堆中
     * @param fd 套接字
     * @param ev 事件
     * @param pattern 事件模式
     */
    void add_socket(int fd, event_t ev, pattern_t pattern) {
        epoll_add(fd, ev, pattern);
    }

    /**
     * @brief 添加套接字（封装在socket.h中的对象）
     * @tparam F 可执行类型
     * @param sock 套接字
     * @param ev 事件类型
     * @param pattern 触发模式
     * @param cb 回调函数
     */
    template <typename F>
    void add_socket(const net::socket_base& sock, event_t ev, pattern_t pattern, F&& cb) {
        cb_list.emplace(std::make_pair<int, std::function<void()>>(sock.get_fd(), std::forward<F>(cb)));
        epoll_add(sock.get_fd(), ev, pattern);
    }

    /**
     * @brief 更新套接字状态（在oneshot模式下使用）
     * @param sock  套接字
     * @param state 套接字状态
     */
    void mod_event(int for_whom, event_t event, pattern_t pattern) {
        epoll_mod(for_whom, event, pattern);
    }

    // 设置可写回调
    template <typename F>
    void set_readable_cb(F&& cb) {
        readable_cb = std::forward<F>(cb);
    }
    // 设置可读事件回调
    template <typename F>
    void set_writable_cb(F&& cb) {
        writable_cb = std::forward<F>(cb);
    }
    // 对端关闭/异常的回调，默认关闭socket
    template <typename F>
    void set_disconnect_cb(F&& cb) {
        disconnect_cb = std::forward<F>(cb);
    }

    /**
     * @brief 激活reactor，并阻塞所在线程
     */
    void activate() {
        int fd = 0;
        int count = 0;
        int event_nums = 0;
        int curr_bufsz = details::EPOLL_EVENT_BUF_SIZE;
        decltype(cb_list)::iterator it(nullptr);
        while (!stop) {
            // 等待事件
            event_nums = epoll_wait(epoll_fd, events.get(), curr_bufsz, -1);
            for (int i = 0; i < event_nums; i++) {
                fd = events[i].data.fd;
                if ((it = cb_list.find(fd)) != cb_list.end()) {
                    it->second();  //执行回调
                } else if (events[i].events & event::disconnect) {
                    if (static_cast<bool>(disconnect_cb)) {
                        disconnect_cb(fd);
                    } else {
                        close(fd);
                    }
                } else if (fd == signal_fd && (events[i].events & event::readable)) {
                    deal_signal();
                } else if (events[i].events & event::readable) {
                    readable_cb(fd);
                } else if (events[i].events & event::writable) {
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

    void shutdown() noexcept {
        if (!stop) {
            stop = true;
            close(epoll_fd);
        }
    }
};


}  // namespace nc::net