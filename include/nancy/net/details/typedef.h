#pragma once
#include <sys/epoll.h>
#include <functional>

namespace nc::net {

// localhost
static const char* localhost = "127.0.0.1";
static const uint8_t TCP_PROTOCOL = SOCK_STREAM;
static const uint8_t UDP_PROTOCOL = SOCK_DGRAM;


// epoll event
using event_t = decltype(epoll_event::events);
struct event {
    static const event_t null = 0;
    static const event_t readable = EPOLLIN;
    static const event_t writable = EPOLLOUT;
    static const event_t disconnect = EPOLLRDHUP | EPOLLHUP | EPOLLERR;
};

// socket's react patterns
using pattern_t = decltype(epoll_event::events);
struct pattern {
    static const pattern_t lt = 0;
    static const pattern_t et = EPOLLET;
    static const pattern_t lt_oneshot = EPOLLONESHOT;
    static const pattern_t et_oneshot = EPOLLET | EPOLLONESHOT;
};


class reactor;

// 回调类型
using socket_callback_t = typename std::function<void(int)>;
using common_callback_t = typename std::function<void()>;
using reactor_socket_callback_t = typename std::function<void(reactor*, int)>;
using reactor_common_callback_t = typename std::function<void(reactor*)>;

}