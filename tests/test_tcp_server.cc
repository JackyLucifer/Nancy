#include <cstring>
#include <iostream>
#include "nancy/net/tcp_server.h"
using namespace std;
using namespace nc;

// ==================================================
//           基于reactor的高效且完善的TCP服务器
// ==================================================

int main() {
    net::tcp_server serv(net::localhost);  //默认端口9090

    // 主动添加socket，触发类型限定为EPOLL_ET_ONESHOT
    serv.set_connect_cb([&](int fd) {
        cout << "connect: " << fd << endl;
        serv.add_socket(fd, net::event::readable);
    });

    serv.set_readable_cb([&](int fd) {
        const int sz = 1024;
        static char buf[sz];
        int ret = read(fd, buf, sz - 1);
        buf[ret] = '\0';
        printf("Content: %s", buf);
        if (ret == sz - 1) {
            serv.add_event(fd, net::event::readable);  // 继续读
        } else {
            serv.add_event(fd, net::event::writable);  // 注册可写事件
        }
    });
    serv.set_writable_cb([&](int fd) {
        const char* hello = "Hello, I'm Nancy\n";
        size_t ret = write(fd, hello, strlen(hello));
        if (ret == strlen(hello)) {  // 发送完毕，继续监听读事件
            serv.add_event(fd, net::event::readable);
        } else {
            cout << "[warn]: The data hasn't been sent completly\n";
            serv.add_event(fd, net::event::readable);  // 继续读
        }
    });
    serv.set_disconnect_cb([](int fd) {
        cout << "disconnect: " << fd << endl;
        close(fd);
    });
    serv.add_signal(SIGINT, [&](/* 信号的回调是唯一的 */) {
        cout << "Receive: signal SIGINT\n";
        serv.shutdown();
    });

    std::cout<<"---------------------------\n";
    std::cout<<"服务器启动..."<<std::endl;
    std::cout<<"IP: "<<net::localhost<<std::endl;
    std::cout<<"Port: "<<net::details::NET_DEFAULT_PORT<<std::endl;
    std::cout<<"---------------------------\n";
    serv.activate();
}