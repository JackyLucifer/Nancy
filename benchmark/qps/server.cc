#include "nancy/net/creactors.h"
using namespace nc;

#define RECV_BYTES 4096
#define SEND_BYTES 16384

int main(int argn, char** args) {
    assert(argn == 2);

    // sock
    net::tcp_serv_socket sok;
    net::set_reuse_address(sok.get_fd());
    sok.listen_req("127.0.0.1", 9090);

    // concurrent reactors
    net::creactors recs;
    recs.bind_serv_socket(std::move(sok));
    recs.init_async_nodes(atoi(args[1]));

    recs.set_connect_cb([](net::reactor* rec, int fd){
        net::set_nonblocking(fd);
        rec->add_socket(fd, net::event::readable, net::pattern::et); 
    });

    char empty_buf[RECV_BYTES];
    char mesg[SEND_BYTES];  // 16k
    memset(mesg, 'x', SEND_BYTES);
    mesg[16383] = 'y';
    std::vector<std::pair<int, int>> info(20000, {0, 0}); // ridx widx
    recs.set_readable_cb([&](net::reactor* rec, int fd) {
        // 读
        int bytes = 0;
        int idx = info[fd].first; 
        while ((bytes = recv(fd, empty_buf+idx, RECV_BYTES-idx, 0)) > 0) {
            idx += bytes;
        }
        if (idx == RECV_BYTES) {
            info[fd].first = 0;
        } else {
            info[fd].first = idx;
            return ;
        }
        // 开始写时idx=0即可
        idx = 0; 
        while ((bytes = send(fd, mesg+idx, SEND_BYTES-idx, 0)) > 0) {
            idx += bytes;
        }
        if (idx == SEND_BYTES) {
            info[fd].second = 0;
        } else {
            info[fd].second = idx;
            rec->mod_event(fd, net::event::writable, net::pattern::et_oneshot);
        }
    });
    recs.set_writable_cb([&](net::reactor* rec, int fd){
        // 继续发送
        int bytes = 0;
        int idx = info[fd].second;
        while ((bytes = send(fd, mesg+idx, SEND_BYTES-idx, 0)) > 0) {
            idx += bytes;
        }
        if (idx == SEND_BYTES) {
            info[fd].second = 0;
            rec->mod_event(fd, net::event::readable, net::pattern::et);
        } else {
            info[fd].second = idx;
            rec->mod_event(fd, net::event::writable, net::pattern::et_oneshot);
        }
    });
    recs.activate();


}