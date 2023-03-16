#include <iostream>
#include "nancy/net/reactor.h"
using namespace nc;

// ================================
//          灵活的reactor
// ================================

int main() {
    net::reactor rec;
    net::tcp_serv_socket serv_sock;

    serv_sock.set_reuse_address();
    serv_sock.set_nonblocking();
    serv_sock.listen_req(net::localhost, 9090);

    auto conn_cb = [&]() {
        int sock = 0;
        while (1) {
            sock = serv_sock.accept_req();  // 调用原socket的accept接收连接
            if (sock > 0) {
                net::set_nonblocking(sock);  // 设置为非阻塞
                rec.add_socket(sock, net::event::readable, net::pattern::et_oneshot);
                std::cout << "connect fd: " << sock << std::endl;
            } else {
                break;
            }
        }
    };
    rec.add_socket(serv_sock, net::event::readable, net::pattern::et, conn_cb);
    // 以上将reactor改造得和tcp_server的属性功能相同

    rec.set_readable_cb([&](int fd) {
        const size_t sz = 1024;
        static char buf[sz];
        int ret = read(fd, buf, sz - 1);
        buf[ret] = '\0';
        std::cout << "receive content: " << buf;
        if (ret == (sz - 1)) {
            rec.add_event(fd, net::event::readable, net::pattern::et_oneshot);
        } else {
            rec.add_event(fd, net::event::writable, net::pattern::et_oneshot);
        }
    });

    rec.set_writable_cb([&](int fd) {
        const char* hello = "Hello, I'm Nancy\n";
        const size_t len = strlen(hello);
        size_t ret = write(fd, hello, len);
        if (ret == len) {  // 发送完毕，继续监听读事件
            rec.add_event(fd, net::event::readable, net::pattern::et_oneshot);
        } else {
            std::cout << "[warn]: The data hasn't been sent completly\n";
            rec.add_event(fd, net::event::readable, net::pattern::et_oneshot);  // 这里仅做简化处理
        }
    });
    rec.set_disconnect_cb([](int fd) {
        close(fd);
        std::cout << "disconnect " << fd << std::endl;
    });
    std::cout<<"服务端启动..."<<std::endl;
    std::cout<<"IP地址: "<<net::localhost<<std::endl;
    std::cout<<"端口号: "<<net::details::NET_DEFAULT_PORT<<std::endl;
    rec.activate();
}
