#pragma once 
#include "nancy/net/reactor.h"
#include "nancy/details/type_traits.h"
#include <vector>
#include <thread>

namespace nc::net {

/**
 * @brief 并发多节点反应堆(concurrent reactors)，采用one-thread-per-loop + 监听端口反应堆分发套接字的模式实现高性能服务
 * @note  接口并未被刻意设计为线程安全
 * @note  除activate外其它接口均为非阻塞的
 * @note  在activate时异步线程才被创建，此前的所有设置才生效。因此回调设置接口在activate前是可重入的。
 * @note  单独为工作节点设置回调函数时，其优先级是更高的
*/
class creactors {

    // 异步反应堆节点
    class async_node {
        net::reactor rec;
        net::sockpair pair;
        std::unique_ptr<char[]> req_buf;
    public:
        static const int bufsz = 128;
        async_node(int timeout)
            : rec(timeout) 
            , pair() {
            req_buf.reset(new char[bufsz]);
        } 
        ~async_node() = default;
    public:
        net::sockpair* channel() {
            return &pair;
        }
        net::reactor*  reactor() {
            return &rec;
        }
        char* buffer() {
            return req_buf.get();
        }
    };
    using node_ptr = std::shared_ptr<async_node>;

private:
    int  cur = 0;   
    bool stop = false;
    bool initialized = false;
    static const char default_node_nums = 4;

    net::reactor root_node;
    net::tcp_serv_socket sock;
    std::vector<int> failures;
    std::vector<node_ptr> nodes;
    std::vector<std::thread> workers;

    net::reactor_callback_t timeout_cb = {};
    net::reactor_socket_callback_t conn_cb = {};
    net::reactor_socket_callback_t readable_cb = {};
    net::reactor_socket_callback_t writable_cb = {};
    net::reactor_socket_callback_t disconnect_cb = {};

public:
    explicit creactors() {}
    ~creactors() { destroy(); }

public:
    auto root() -> reactor* {
        return &root_node;
    }

    /**
     * @brief 绑定服务端套接字。自动设置为非阻塞，其它属性不做更改，但原套接字失效。
     * @param sock 服务端套接字
     */
    void bind_serv_socket(tcp_serv_socket&& tmp) { 
        sock = std::move(tmp);
        net::set_nonblocking(sock.get_fd());
        root_node.add_socket(sock.get_fd(), event::readable, pattern::et, [this](int){ 
            int fd = 0;
            int bytes = 0;
            int old = cur;
            while ((fd = sock.accept_req()) > 0) {
                while ((bytes = nodes[cur]->channel()->write_lfd((char*)&fd, sizeof(uint16_t))) != 2) {
                    cur = (cur + 1) % nodes.size();  // 向前查找
                    if (cur == old) {
                        failures.push_back(fd);      // 推入溢出缓冲区的文件描述符
                    }
                } 
                cur = (cur+1) % nodes.size();        // 负载均衡
                old = cur;
            }
        });
        initialized = true;
    }
    
    /**    
     * @brief 初始化异步工作节点。如果不进行初始化，creactors的工作节点数被设置为4
     * @param nums 异步工作节点个数
     * @param timeout 异步工作节点(reactor)的超时时间，单位(毫秒)，默认没有超时限制
     */
    void init_async_nodes(int nums, int timeout = -1) {
        assert(nums > 0);
        for (int i = 0; i < nums; ++i) {
            nodes.emplace_back(new async_node(timeout));
        }
        
    }

    /**
     * @brief 判断是否存在添加失败的fd
     * @return 溢出的文件描述符
     */
    bool exist_failure_fd() {
        return failures.size();
    }

    /**
     * @brief 同上，获取失败的文件描述符
     * @return 返回保存溢出描述符的vector
     */
    auto get_failure_fds()->std::vector<int>& {
        return failures;
    }

    /**
     * @brief 设置节点统一的连接回调，主要用于制定触发模式，否则连接的描述符为et模式并注册可读事件（常用模式）
     * @tparam F 可执行对象
     * @param cb 回调类型为void(reactor*, int);
     */
    template <typename F, typename = typename std::enable_if<nc::details::is_runnable<F, reactor*, int>::value>::type>
    void set_connect_cb(F&& cb) {
        conn_cb = std::forward<F>(cb);
    }

    /**
     * @brief 工作节点统一可读回调
     * @tparam F 可执行对象，参数为int
     * @param cb 回调
     */
    template <typename F, typename = typename std::enable_if<nc::details::is_runnable<F, reactor*, int>::value>::type>
    void set_readable_cb(F&& cb) {
        readable_cb = std::forward<F>(cb);
    }

    /**
     * @brief 工作节点统一可写回调
     * @tparam F 可执行对象，参数为int
     * @param cb 回调
     */
    template <typename F, typename = typename std::enable_if<nc::details::is_runnable<F, reactor*, int>::value>::type>
    void set_writable_cb(F&& cb) {
        writable_cb = std::forward<F>(cb);
    }

    /**
     * @brief 工作节点统一超时回调
     * @tparam F 可执行对象，参数为int
     * @param cb 回调
     */
    template <typename F, typename = typename std::enable_if<nc::details::is_runnable<F, reactor*>::value>::type>
    void set_timeout_cb(F&& cb) {
        timeout_cb = std::forward<F>(cb);
    }

    /**
     * @brief 工作节点统一连接断开回调
     * @tparam F 可执行对象，参数为int
     * @param cb 回调
     */
    template <typename F, typename = typename std::enable_if<nc::details::is_runnable<F, reactor*, int>::value>::type>
    void set_disconnect_cb(F&& cb) {
        disconnect_cb = std::forward<F>(cb);
    }

    /**
     * @brief 激活反应堆
     * @note 若未初始化异步节点，则默认生成default_node_nums个节点
     */
    void activate() {
        assert(initialized == true);
        if (nodes.empty()) {
            init_async_nodes(default_node_nums);
        }
        create_threads();
        root_node.activate();
    }

    /**
     * @brief 主动关闭并发反应堆（非阻塞），否则在析构函数关闭
     * @note 该函数指示异步反应堆关闭并将线程分离以完成最后的工作
     */
    void destroy() {
        if (stop) {
            stop = true;
            root_node.destroy();
            for (auto& ptr: nodes) {
                ptr->reactor()->destroy();  
            }
            for (auto& thrd: workers) {
                thrd.detach();
            }
        }
    }   

public:

    size_t node_nums() {
        return nodes.size();
    }

    auto operator[](unsigned idx) -> reactor* {
        return nodes[idx]->reactor();
    }


private:
    // 创建工作线程
    void create_threads() {
        for (size_t i = 0; i < nodes.size(); ++i) {
            workers.emplace_back(&creactors::worker, this, nodes[i]);
        }
    }

    // 工作线程
    void worker(node_ptr context) {
        auto* rec = context->reactor();
        char* buf = context->buffer();
        int bufsz = async_node::bufsz;
        int notify_fd = context->channel()->get_rfd();
        net::set_nonblocking(notify_fd);  

        if (!conn_cb) {
            conn_cb = [](reactor* rec, int fd){
                set_nonblocking(fd); 
                rec->add_socket(fd, event::readable, pattern::et);
            };
        }
        // init pipe
        rec->add_socket(notify_fd, event::readable, pattern::et, [&](int){
            int bytes = 0;
            while ((bytes = read(notify_fd, buf, bufsz)) > 0) {
                auto array = (uint16_t*)(buf);
                for (int i = 0; i < (bytes/2); ++i) {
                    conn_cb(rec, (int)array[i]);
                }
            }
        });

        // 以定制的回调为更高优先级
        if (readable_cb && !rec->get_readable_cb()) {
            auto tmp_rdable_cb = readable_cb;
            rec->set_readable_cb([tmp_rdable_cb, rec](int fd){ tmp_rdable_cb(rec, fd); });
        } 
        if (writable_cb && !rec->get_writable_cb()) {
            auto tmp_wtable_cb = writable_cb;
            rec->set_writable_cb([tmp_wtable_cb, rec](int fd){ tmp_wtable_cb(rec, fd); });
        }
        if (disconnect_cb && !rec->get_disconnect_cb()) {
            auto tmp_disconn_cb = disconnect_cb;
            rec->set_disconnect_cb([tmp_disconn_cb, rec](int fd){ tmp_disconn_cb(rec, fd); });
        }
        if (timeout_cb && !rec->get_timeout_cb()) {
            auto tmp_tout_cb = timeout_cb;
            rec->set_timeout_cb([tmp_tout_cb, rec](){ tmp_tout_cb(rec); });
        }

        // 激活反应堆
        rec->activate();
    }

};





} // namespace nc::net