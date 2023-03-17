#include "nancy/net/tcp_server.h"

int main() {
    nc::net::tcp_server serv(nc::net::localhost);  // port: 9090
    const int buf_sz = 1025;
    char buf[buf_sz];
    serv.set_readable_cb([&](int fd) {
        int rcv = 0;
        while ((rcv = read(fd, buf, buf_sz - 1)) > 0) {
            write(fd, buf, rcv);
        }
        serv.add_event(fd, nc::net::event::readable);
    });
    serv.activate();
}
