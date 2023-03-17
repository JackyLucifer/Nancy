#include "nancy/net/reactor.h"
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <condition_variable>
#include <chrono>
using namespace nc;

// 具体pingpong细节
int conn_nums = 0;
const int mesg_sz = 16*1024; 

// 线程通知机制
int thread_done_nums = 0;
std::mutex cv_lok;
std::condition_variable thread_done_cv;

std::atomic<double> time_start;
std::atomic<uint64_t> bytes_collect = {0};

// 让输出工整
std::mutex out_lok;

double get_absolute_time() {
    std::chrono::duration<double> timestamp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return timestamp.count();
}

void mesg_sender () {
    net::reactor rec;
    std::vector<net::tcp_clnt_socket> socks(conn_nums); // 防止直接析构关闭了套接字
    int connected = 0;

    for (int i = 0; i < conn_nums; ++i) {
        int fd = socks[i].get_fd();
        assert(net::get_send_bufsz(fd) >= mesg_sz); // 避免缓冲区过小影响测量结果
        net::set_tcp_nondelay(fd);
        socks[i].launch_req("127.0.0.1", 9090);  
        net::set_nonblocking(fd);
        connected++;
        rec.add_socket(fd, net::event::writable, net::pattern::lt);
    }
    {
        std::lock_guard<std::mutex> lock(out_lok);
        std::cout<<"tid: "<<std::this_thread::get_id()<<" conn: "<<connected<<std::endl;
    }

    uint64_t total_send_bytes = 0;
    char buffer[mesg_sz];
    memset(buffer, mesg_sz, 'x');  // message

    // 不断发送数据
    rec.set_writable_cb([&](int fd) {
        int tmp = 0;
        int send_bytes = 0;
        while ((tmp = send(fd, buffer+send_bytes, mesg_sz-send_bytes, 0)) > 0) {
            send_bytes += tmp;
        }
        total_send_bytes += send_bytes;
    });

    // 负责退出打印和记录信息
    rec.set_disconnect_cb([&](int fd){
        if (--connected == 0) {  // 全部连接已经断开
            {
                std::lock_guard<std::mutex> lock(out_lok);
                std::cout<<"tid: "<<std::this_thread::get_id()<<"send bytes: "<<total_send_bytes<<std::endl;
                bytes_collect.fetch_add(total_send_bytes, std::memory_order_relaxed); // 全双工模式
            }
            {
                std::unique_lock<std::mutex> lock(cv_lok);
                thread_done_nums++;
            }
            thread_done_cv.notify_one();
            rec.shutdown(); // 关闭reactor
        } 
    });
    time_start.store(get_absolute_time());
    rec.activate();
}

int main(int argn, char** args) {

    assert(argn == 3);
    int tnums = atoi(args[1]);   // 线程数
    conn_nums = atoi(args[2]);   // 每条线程的连接数

    int fd = net::signal_socket_init();   // 初始化信号机制以屏蔽SIGPIPE
    net::signal_add(SIGPIPE);
    // 当信号传来时我们忽略它(不处理fd即可)

    std::vector<std::thread> threadpool;
    for (int i = 0; i < tnums; ++i) {
        threadpool.emplace_back(mesg_sender);    
    }
    for (auto& t: threadpool) {
        t.detach();
    }
    std::unique_lock<std::mutex> lock(cv_lok);
    thread_done_cv.wait(lock, [&tnums]{return thread_done_nums == tnums;});

    auto time_cost = get_absolute_time() - time_start.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待其它IO输出
    std::cout<<"Total time cost: "<<time_cost<<" seconds"<<std::endl;
    double mb = ((double)bytes_collect.load()/(double)(1024*1024));
    std::cout<<"Handling capacity of Nancy: "<< mb/time_cost<<" (mb/s)"<< std::endl;
   

}
