#pragma once
#include <functional>
#include <vector>
#include <thread>

namespace nc::net {

// 信号服务目前是单例模式

// 初始化信号服务，返回读信号端socket
int  signal_socket_init();

// 添加信号
void signal_add(int sig);

// 关闭信号服务
void signal_socket_close();


}  // namespace nc::net::_net