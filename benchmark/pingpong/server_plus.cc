#include "nancy/net/creactors.h"
#include <cstdlib>
#include <iostream>
using namespace nc;

int main(int argn, char** args) {
    assert(argn == 2);
    net::tcp_serv_socket sock;
    net::set_reuse_address(sock.get_fd());
    net::set_tcp_nondelay(sock.get_fd());
    sock.listen_req("127.0.0.1", 9090);

    net::creactors recs;
    recs.bind_serv_socket(std::move(sock));
    recs.init_async_nodes(atoi(args[1]));  // 初始化异步工作节点

    // 统一设置工作节点 
    recs.set_connect_cb([](net::reactor* rec, int fd){
        net::set_nonblocking(fd);
        rec->add_socket(fd, net::event::readable, net::pattern::lt);
    });

    int mesg_sz = 16*1024;
    char buffer[mesg_sz];
    recs.set_readable_cb([&buffer, &mesg_sz](int fd){
        int tmp = 0;
        int recv_bytes = 0;
        while ((tmp = recv(fd, buffer, mesg_sz-recv_bytes, 0)) > 0) { // 读取16k
            recv_bytes += tmp;
        } 
    });
    recs.activate();
}