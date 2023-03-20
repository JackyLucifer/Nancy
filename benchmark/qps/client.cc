#include "nancy/net/reactor.h"
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <condition_variable>
#include <chrono>
using namespace nc;

#define RECV_BYTES 16384
#define SEND_BYTES 4096

// 连接数
int conn_nums = 0;

// 线程通知机制
int thr_nums = 0;
int thr_done_nums = 0;
std::atomic_int thr_start_nums = {0};
std::mutex cv_lok;
std::condition_variable thread_done_cv;

std::atomic<double> time_start;
std::atomic<uint64_t> bytes_collect = {0};
std::atomic<uint32_t> requests_collect = {0};

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
        assert(net::get_send_bufsz(fd) >= SEND_BYTES); // 避免缓冲区过小影响测量结果
        socks[i].launch_req("127.0.0.1", 9090);  
        net::set_nonblocking(fd);  // 非阻塞IO
        connected++;
        rec.add_socket(fd, net::event::writable, net::pattern::et_oneshot);
    }
    {
        std::lock_guard<std::mutex> lock(out_lok);
        std::cout<<"["<<std::this_thread::get_id()<<"] conn: "<<connected<<std::endl;
    } 
    thr_start_nums++;
    while (thr_start_nums < thr_nums) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 等待所有线程连接完毕
    } 

    uint64_t total_bytes = 0;
    uint32_t requests = 0;

    // message, buffer, info, count
    char mesg[SEND_BYTES];
    memset(mesg, SEND_BYTES, 'y');  
    mesg[SEND_BYTES-1] = 'x';
    
    char empty_buf[RECV_BYTES];
    std::vector<std::pair<int, int>> info(20000, {0,0}); // ridx, widx


    // 发送数据
    rec.set_writable_cb([&](int fd) {
        int bytes = 0;
        int idx = info[fd].second;
        while ((bytes = send(fd, mesg+idx, SEND_BYTES-idx, 0)) > 0) {
            idx += bytes;
        }
        if (idx == SEND_BYTES) {
            info[fd].second = 0;
            rec.mod_event(fd, net::event::readable, net::pattern::et);
            total_bytes += SEND_BYTES;
        } else {
            info[fd].second = idx;
            rec.mod_event(fd, net::event::writable, net::pattern::et_oneshot); 
        }
    });
    // 读取16k
    rec.set_readable_cb([&](int fd) {
        int bytes = 0;
        int idx = info[fd].first;
        while ((bytes = recv(fd, empty_buf+idx, RECV_BYTES-idx, 0)) > 0) {
            idx += bytes;
        }
        if (idx == RECV_BYTES) {
            info[fd].first = 0;
            total_bytes += RECV_BYTES;
            requests += 1;
            rec.mod_event(fd, net::event::writable, net::pattern::et_oneshot);
        } else {
            info[fd].first = idx;
            return ;
        }
    });


    // 负责退出打印和记录信息
    rec.set_disconnect_cb([&](int fd){
        if (--connected == 0) {  // 全部连接已经断开
            {
                std::lock_guard<std::mutex> lock(out_lok);
                std::cout<<"["<<std::this_thread::get_id()<<"] exchange bytes: "<<total_bytes<<std::endl;
                bytes_collect.fetch_add(total_bytes, std::memory_order_relaxed);  // 交换的字节数
                requests_collect.fetch_add(requests, std::memory_order_relaxed);  // 请求完成数
            }
            {
                std::unique_lock<std::mutex> lock(cv_lok);
                thr_done_nums++;
            }
            thread_done_cv.notify_one();
            rec.destroy(); // 销毁reactor
        } 
    });
    time_start.store(get_absolute_time());
    rec.activate();
}

int main(int argn, char** args) {

    assert(argn == 3);
    thr_nums = atoi(args[1]);    // 线程数
    conn_nums = atoi(args[2]);   // 每条线程的连接数

    net::signal_socket_init();   // 初始化信号机制以屏蔽SIGPIPE
    net::signal_add(SIGPIPE);
    // 当信号传来时我们忽略它(不处理fd即可)

    std::vector<std::thread> threadpool;
    for (int i = 0; i < thr_nums; ++i) {
        threadpool.emplace_back(mesg_sender);    
    }
    std::unique_lock<std::mutex> lock(cv_lok);
    thread_done_cv.wait(lock, []{return thr_done_nums == thr_nums;});

    auto time_cost = get_absolute_time() - time_start.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待其它IO输出
    std::cout<<"Total time cost: "<<time_cost<<" seconds"<<std::endl;
    double mb = ((double)bytes_collect.load()/(double)(1024*1024));
    std::cout<<"Bytes exchange rate: "<< mb/time_cost<<" (mb/s)"<< std::endl;
    std::cout<<"QPS of Nancy: "<<requests_collect.load()/time_cost<<"(req/s)"<<std::endl;
   
    for (auto& t: threadpool) {
        t.detach();
    }
}