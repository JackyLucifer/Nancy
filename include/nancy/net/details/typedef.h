#pragma once
#include <sys/epoll.h>

namespace nc::net {

// localhost
static const char* localhost = "127.0.0.1";

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
    static const pattern_t et_oneshot = EPOLLET | EPOLLONESHOT;
};


}