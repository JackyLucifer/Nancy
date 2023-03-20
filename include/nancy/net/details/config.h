#pragma once

namespace nc::net::details {

// =====================  reactor ==========================

static const int NET_DEFAULT_PORT = 9090;      // 默认的端口号
static const int EPOLL_CREATE_SIZE = 1024;     // hint for epoll
static const int EPOLL_EVENT_BUF_SIZE = 1024;  // epoll events[]的初始大小，即支持epoll最高的单次事件数
static const int TCP_LISTEN_QUEUE_LEN = 30;    // listen的backlog参数

// ====================  reactor ==========================


}  // namespace  nc::_nc