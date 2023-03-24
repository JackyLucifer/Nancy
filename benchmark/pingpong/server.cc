#include "nancy/net/reactor.h"
#include <iostream>
#include <chrono>
#include <memory>
#include <vector>
using namespace nc;

const int mesg_sz = 16*1024; 

int main() {
    net::reactor rec;
    net::tcp_serv_socket serv_sock;

    net::set_nonblocking(serv_sock.get_fd());
    net::set_reuse_address(serv_sock.get_fd());
    assert(net::get_recv_bufsz(serv_sock.get_fd()) >= mesg_sz); // 避免缓冲区过小影响测量结果
    serv_sock.listen_req("127.0.0.1", 9090);

    auto conn_cb = [&](int) {
        int sock = 0;
        while (1) {
            sock = serv_sock.accept_req();
            if (sock > 0) {
                net::set_nonblocking(sock);  // 设置为非阻塞
                rec.add_socket(sock, net::event::readable, net::pattern::lt);  // 采用LT模式
            } else {
                break;
            }
        }
    };
    rec.add_socket(serv_sock.get_fd(), net::event::readable, net::pattern::et, conn_cb); 
    
    char buffer[mesg_sz];
    uint64_t total_recv_bytes = 0;

    // 尝试读16k
    rec.set_readable_cb([&buffer, &total_recv_bytes](int fd){
        int tmp = 0;
        int recv_bytes = 0;
        while ((tmp = recv(fd, buffer, mesg_sz-recv_bytes, 0)) > 0) {
            recv_bytes += tmp;
        } 
        total_recv_bytes += recv_bytes;
    });

    rec.add_signal(SIGINT, [&rec, &total_recv_bytes](int){
        std::cout<<"\nTotal bytes: "<<total_recv_bytes<<"  -> "<<(double)total_recv_bytes/(1024*1024)<<"MB"<<std::endl;
        rec.destroy();
    });
    rec.activate();
}