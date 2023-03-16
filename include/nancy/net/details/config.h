#pragma once

namespace nc::net::details {

// =====================  reactor ==========================

static const int NET_DEFAULT_PORT = 9090;
static const int EPOLL_CREATE_SIZE = 1024;
static const int EPOLL_EVENT_BUF_SIZE = 1024;  // epoll events[]的初始大小
static const int TCP_LISTEN_QUEUE_LEN = 30;    // listen的backlog参数

// ====================  reactor ==========================


}  // namespace  nc::_nc