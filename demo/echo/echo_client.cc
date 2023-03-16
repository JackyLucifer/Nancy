#include <iostream>
#include "nancy/net/socket.h"
using namespace nc;

int main() {
    const char* hello = "Hello Nancy";
    net::tcp_clnt_socket clnt;

    clnt.launch_req(net::localhost, 9090);
    write(clnt.get_fd(), hello, strlen(hello));

    char buf[128];
    int ret = read(clnt.get_fd(), buf, 128);
    buf[ret] = '\0';
    std::cout << "recv: " << buf << std::endl;
}