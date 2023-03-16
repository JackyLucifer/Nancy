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
    net::set_tcp_nondelay(serv_sock.get_fd());  // 禁止Nagle
    net::set_reuse_address(serv_sock.get_fd());
    assert(net::get_recv_bufsz(serv_sock.get_fd()) >= mesg_sz); // 避免缓冲区过小影响测量结果
    serv_sock.listen_req(net::localhost, 9090);

    auto conn_cb = [&]() {
        int sock = 0;
        while (1) {
            sock = serv_sock.accept_req();
            if (sock > 0) {
                net::set_nonblocking(sock);  // 设置为非阻塞
                rec.add_socket(sock, net::event::readable, net::pattern::et);  // 采用单线程采用ET模式即可
            } else {
                break;
            }
        }
    };
    rec.add_socket(std::move(serv_sock), net::event::readable, net::pattern::et, conn_cb); 
    std::unique_ptr<char[]> buffer(new char[mesg_sz]); 
    
    uint64_t total_bytes = 0;
    rec.set_readable_cb([&](int fd){
        int tmp = 0;
        int recv_bytes = 0;
        int send_bytes = 0;
        while ((tmp = recv(fd, buffer.get(), mesg_sz-recv_bytes, 0)) > 0) {
            recv_bytes += tmp;
        } // 尝试读16k
        while ((tmp = send(fd, buffer.get(), mesg_sz-send_bytes, 0)) > 0) {
            send_bytes += tmp;
        } // 尝试写16k

        total_bytes += recv_bytes;
        total_bytes += send_bytes;
    });

    // 用kill -KILL 杀死程序
    int count = 2;
    rec.add_signal(SIGINT, [&rec, &total_bytes, &count]{
        std::cout<<"\nTotal bytes: "<<total_bytes<<"  -> "<<(double)total_bytes/(1024*1024)<<"MB"<<std::endl;
        rec.shutdown();
    });
    rec.activate();
}