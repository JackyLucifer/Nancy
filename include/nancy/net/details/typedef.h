#pragma once
#include <sys/epoll.h>
#include <functional>

namespace nc::net {

// localhost
static const uint8_t TCP_PROTOCOL = SOCK_STREAM;
static const uint8_t UDP_PROTOCOL = SOCK_DGRAM;


// epoll event
using event_t = decltype(epoll_event::events);
namespace event {
    static const event_t null = 0;
    static const event_t readable = EPOLLIN; 
    static const event_t writable = EPOLLOUT;
    static const event_t disconnect = EPOLLRDHUP|EPOLLHUP|EPOLLERR;
};

// socket's react patterns
using pattern_t = decltype(epoll_event::events);
namespace pattern {
    static const pattern_t lt = 0;
    static const pattern_t et = EPOLLET;
    static const pattern_t lt_oneshot = EPOLLONESHOT;
    static const pattern_t et_oneshot = EPOLLET | EPOLLONESHOT;
};


class reactor;

// 回调类型
using callback_t = typename std::function<void()>;
using socket_callback_t = typename std::function<void(int)>;
using reactor_socket_callback_t = typename std::function<void(reactor*, int)>;
using reactor_callback_t = typename std::function<void(reactor*)>;


}