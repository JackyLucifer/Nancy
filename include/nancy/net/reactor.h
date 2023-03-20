#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

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
#include "nancy/details/type_traits.h"

namespace nc::net {  

/**
 * @brief 可定制不同触发模式和设置事件回调的反应堆
 * @note  配置文件在 ./details/config.h。事实上所做的配置并不需要多做更改
*/
class reactor {
    bool stop = false;
    int epoll_fd = 0;
    int signal_fd = 0;
    int timeout = -1;

    std::unique_ptr<epoll_event[]> events = {nullptr};
    socket_callback_t readable_cb = {};
    socket_callback_t writable_cb = {};
    socket_callback_t disconnect_cb = {};
    common_callback_t timeout_cb = {};
    std::unordered_map<int, socket_callback_t> cb_list = {};
    std::map<int, socket_callback_t> signal_cbs = {};

public:
    explicit reactor(int timeout = -1) {
        assert((epoll_fd = epoll_create(details::EPOLL_CREATE_SIZE)) != -1);
        events.reset(new epoll_event[details::EPOLL_EVENT_BUF_SIZE]);
        this->timeout = timeout;
    }
    ~reactor() noexcept {
        destroy();
    }

private:
    void epoll_add(int sock, event_t ev, pattern_t pattern) {
        struct epoll_event event;
        event.data.fd = sock;
        event.events = ev | pattern | event::disconnect;
        if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event)) {
            std::cout<<"failed"<<std::endl;
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
            signal_cbs[signals[i]](signals[i]);
        }
    }

public:
    // 注册信号
    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F, int>::value>::type>
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
     * @brief 添加文件描述符到反应堆中
     * @param fd 文件描述符
     * @param ev 事件
     * @param pattern 事件模式（Epoll的触发模式）
     */
    void add_socket(int fd, event_t ev, pattern_t pattern) {
        epoll_add(fd, ev, pattern);
    }

    /**
     * @brief 添加文件描述符到反应堆中
     * @tparam F 可执行对象
     * @param fd 文件描述符
     * @param ev 事件
     * @param pattern 事件模式
     * @param cb 回调函数
     */
    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F, int>::value>::type>
    void add_socket(int fd, event_t ev, pattern_t pattern, F&& cb) {
        cb_list.emplace(fd, std::forward<F>(cb));
        epoll_add(fd, ev, pattern);
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
    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F, int>::value>::type>
    void set_readable_cb(F&& cb) {
        readable_cb = std::forward<F>(cb);
    }
    
    // 设置可读事件回调
    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F, int>::value>::type>
    void set_writable_cb(F&& cb) {
        writable_cb = std::forward<F>(cb);
    }
    
    // 对端关闭/异常的回调，默认关闭socket
    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F, int>::value>::type>
    void set_disconnect_cb(F&& cb) {
        disconnect_cb = std::forward<F>(cb);
    }

    template <typename F,  typename = typename std::enable_if<nc::details::is_runnable<F>::value>::type>
    void set_timeout_cb(F&& cb) {
        timeout_cb = std::forward<F>(cb);
    }

    /**
     * @brief 重置超时时间
     * @param timeout 
     */
    void reset_timeout(int timeout) {
        this->timeout = timeout;
    }

    // 获取可读事件的回调函数的引用
    auto get_readable_cb() -> const socket_callback_t& {
        return readable_cb;
    }

    // 获取可写事件的回调函数的引用
    auto get_writable_cb() -> const socket_callback_t& {
        return writable_cb;
    }

    // 获取连接关闭的回调函数的引用
    auto get_disconnect_cb()-> const socket_callback_t& {
        return disconnect_cb;
    }

    // 获取超时回调函数的引用
    auto get_timeout_cb() -> const common_callback_t& {
        return timeout_cb;
    }


    /**
     * @brief 激活reactor，并阻塞所在线程
     */
    void activate() {
        int fd = 0;
        int event_nums = 0;
        decltype(cb_list)::iterator it(nullptr);
        while (!stop) {
            event_nums = epoll_wait(epoll_fd, events.get(), details::EPOLL_EVENT_BUF_SIZE, timeout);
            if (!event_nums) 
                timeout_cb();
            for (int i = 0; i < event_nums; i++) {
                fd = events[i].data.fd;
                if ((it = cb_list.find(fd)) != cb_list.end()) {
                    it->second(fd);  
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
        }
    }

    /**
     * @brief 关闭反应堆，能否立即执行要取决于timeout的设定和目前的执行状态等
     */
    void destroy() noexcept {
        if (!stop) {
            stop = true;
            close(epoll_fd);
        }
    }
};


}  // namespace nc::net