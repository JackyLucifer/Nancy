#pragma once
#include <tuple>
#include <cstring>
#include <ctime>
#include <chrono>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>
#include <fstream>
#include <vector>

// 内存池
#include "nancy/base/memorys.h"

namespace nc::log {


namespace details {

enum class LOG_LEVEL : uint8_t { 
    INFO, WARN, CRIT 
};

typedef std::tuple<char*, const char*, int32_t, char, uint64_t, double, uint32_t, int64_t> support_types;  // 支持的类型
static const support_types type_tuple;

template <typename T, typename Tuple>
struct tuple_index;

template <typename T, typename... Types>
struct tuple_index<T, std::tuple<T, Types...>> {
    static constexpr const uint8_t value = 0;
};

template <typename T, typename U, typename... Types>
struct tuple_index<T, std::tuple<U, Types...>> {
    static constexpr const uint8_t value = 1 + tuple_index<T, std::tuple<Types...>>::value;
};

// 压缩的行

static const int zipline_stack_bufsz = 256;

class zipline {
    size_t bytes_used = 0;
    size_t buf_sz = zipline_stack_bufsz;
    char stack_buf[zipline_stack_bufsz];
    std::unique_ptr<char[]> heap_buf = {nullptr};

public:
    zipline() = default;
    zipline(LOG_LEVEL level, const char *file, const char *function, uint32_t line) {
        encode<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
        encode<std::thread::id>(std::this_thread::get_id());
        encode<const char*>(file);
        encode<const char*>(function);
        encode<uint32_t>(line);
        encode<LOG_LEVEL>(level);
    }
    zipline(const zipline&) = delete;
    zipline& operator = (const zipline& other)  = delete; 
    zipline(zipline&& other) noexcept = default;
    zipline& operator = (zipline&& other) = default; 
    ~zipline() = default;

private:

    char* buffer() {
        return (bool)heap_buf ? (heap_buf.get()+bytes_used) : (stack_buf+bytes_used);
    }

    // 编码 char*
    void encode_cstr(char* arg, size_t len) {
        adapt_buffer(1+len+1);  // type + len + '\0'
        char *b = buffer();
        auto type_id = details::tuple_index<char*, details::support_types>::value;
        *reinterpret_cast<uint8_t*>(b++) = static_cast<uint8_t>(type_id);
        memcpy(b, arg, len);
        *(b + len) = '\0';      // 填充'\0' 
        bytes_used += 1+len+1;
    }

    void encode(char* arg) {
        if (!arg) return;
        else encode_cstr(arg, strlen(arg));
    }

    // 编码 const char* + else
    template <typename Arg>
    void encode(Arg arg) {
        memcpy(buffer(), &arg, sizeof(Arg));
        bytes_used += sizeof(Arg);
    }
    // 编码
    template <typename Arg>
    void encode(Arg arg, uint8_t type_idx) {
        static const size_t type_info_sz = sizeof(uint8_t); 
        adapt_buffer(sizeof(arg) + type_info_sz);
        encode<uint8_t>(type_idx);
        encode(arg);
    }

    // 解码输出
    template <typename Arg>
    char* decode(std::ostream& os, char* b, Arg* dummy) {
        Arg arg = *((Arg*)b);
        os << arg;
        return b + sizeof(Arg);
    }

    // char* 因为无法确认其是否安全，因此采用拷贝后输出字符的方式的形式
    char* decode(std::ostream& os, char* b, char** dummy) {
        while (*b != '\0') {
            os.put(*b);
            ++b;
        }
        return ++b;
    }

    // 调整缓冲
    void adapt_buffer(size_t need_sz) noexcept(false) {
        size_t req_sz =  need_sz + bytes_used;
        if (req_sz <= buf_sz) {
            return;
        }
        buf_sz = (req_sz/512 + 1) * 512;
        auto old_buf = (bool)heap_buf ? heap_buf.get() : stack_buf;
        auto new_buf = new char[buf_sz];
        memcpy(new_buf, old_buf, bytes_used);
        heap_buf.reset(new_buf);
    }

    const char* level_to_cstring(LOG_LEVEL level) {
        switch (level) {
            case LOG_LEVEL::INFO:
                return "INFO";
            case LOG_LEVEL::WARN:
                return "WARN";
            case LOG_LEVEL::CRIT:
                return "CRIT"; 
        }
        return "xxxx"; //undefine
    }   

    // 记录时间
    void write_time(std::ostream& os, uint64_t timestamp) {
        std::time_t time_t = timestamp / 1000000;
        auto gmtime = std::gmtime(&time_t);
        char buf[32];
        strftime(buf, 32, "%Y-%m-%d %T.", gmtime);
        char microseconds[7];
        sprintf(microseconds, "%06lu", timestamp % 1000000);
        os << '[' << buf << microseconds << ']';
    }

    // 解析并发送
    void write_out(std::ostream& os, char* start, const char* const end) {
        if (start == end) return;
        uint8_t type_id = static_cast<uint8_t>(*start);
        start++;
        switch (type_id) {
            case 0: 
                write_out(os, decode(os, start, static_cast<std::tuple_element<0, details::support_types>::type *>(nullptr)), end);
                return;
            case 1: {
                write_out(os, decode(os, start, static_cast<std::tuple_element<1, details::support_types>::type *>(nullptr)), end);
                return;
                }
            case 2:
                write_out(os, decode(os, start, static_cast<std::tuple_element<2, details::support_types>::type *>(nullptr)), end);
                return;
            case 3:
                write_out(os, decode(os, start, static_cast<std::tuple_element<3, details::support_types>::type *>(nullptr)), end);
                return;
            case 4:
                write_out(os, decode(os, start, static_cast<std::tuple_element<4, details::support_types>::type *>(nullptr)), end);
                return;
            case 5:
                write_out(os, decode(os, start, static_cast<std::tuple_element<5, details::support_types>::type *>(nullptr)), end);
                return;
            case 6: 
                write_out(os, decode(os, start, static_cast<std::tuple_element<6, details::support_types>::type *>(nullptr)), end);
                return;
            case 7:
                write_out(os, decode(os, start, static_cast<std::tuple_element<7, details::support_types>::type *>(nullptr)), end);
                return;
        }
    }

public:
    // 写入流中
    void write_into(std::ostream& os) {
        char* b = (bool)heap_buf ? heap_buf.get() : stack_buf;
        char const *const end = b + bytes_used;
        
        // 基本信息
        uint64_t timestamp = *reinterpret_cast<uint64_t *>(b);
        b += sizeof(uint64_t);
        std::thread::id tid = *reinterpret_cast<std::thread::id*>(b);
        b += sizeof(std::thread::id);
        const char* file = *((const char**)b);
        b += sizeof(const char*);
        const char* function = *((const char**)b);
        b += sizeof(const char*);
        uint32_t line = *reinterpret_cast<uint32_t *>(b);
        b += sizeof(uint32_t);
        LOG_LEVEL level = *reinterpret_cast<LOG_LEVEL*>(b);
        b += sizeof(LOG_LEVEL);

        write_time(os, timestamp);
        os << '[' << level_to_cstring(level) << ']'
           << '[' << tid << ']'
           << '[' << file << ':' << function << ':' << line << "] ";
        write_out(os, b, end);
        os << '\n';
        if (level >= LOG_LEVEL::CRIT) {
            os.flush();
        }
    }

    // 捕获变量 const char*
    template <typename T>
    auto operator<<(const T& arg) -> typename std::enable_if<std::is_same<const char*, T>::value, zipline&>::type {
        encode((char*)arg);
        return *this;
    }

    // 捕获变量 char*
    template <typename T>
    auto operator<<(const T& arg) -> typename std::enable_if<std::is_same<char*, T>::value, zipline&>::type {
        encode(arg);
        return *this;
    }

    // 捕获字符串字面值（意外捕获: 局部const char[]）
    template <size_t N>
    zipline &operator<<(const char (&arg)[N]) {
        if (N > sizeof(const char*)) {     // 压缩
            encode((const char*)arg, details::tuple_index<const char*, details::support_types>::value);
        } else {
            encode_cstr((char*)arg, N-1);  // 拷贝
        }
        return *this;
    }

    // 捕获 char[]
    template <size_t N>
    zipline &operator<<(char (&arg)[N]) {
        encode_cstr((char*)arg, N-1);  // 拷贝
        return *this;
    }

    // ===============
    //    其它捕获
    // ===============

    zipline& operator<<(std::string const& arg) {
        encode_cstr((char*)arg.c_str(), arg.length());
        return *this;
    }

    zipline& operator<<(int32_t arg) {
        encode<int32_t>(arg, details::tuple_index<int32_t, details::support_types>::value);
        return *this;
    }

    zipline& operator<<(uint32_t arg) {
        encode<uint32_t>(arg, details::tuple_index<uint32_t, details::support_types>::value);
        return *this;
    }

    zipline& operator<<(int64_t arg) {
        encode<int64_t>(arg, details::tuple_index<int64_t, details::support_types>::value);
        return *this;
    }

    zipline& operator<<(uint64_t arg) {
        encode<uint64_t>(arg, details::tuple_index<uint64_t, details::support_types>::value);
        return *this;
    }

    zipline& operator<<(double arg) {
        encode<double>(arg, details::tuple_index<double, details::support_types>::value);
        return *this;
    }

    zipline& operator<<(char arg) {
        encode<char>(arg, details::tuple_index<char, details::support_types>::value);
        return *this;
    }

};

// 自动滚动文件的文件管理
class file_writter {
    int file_number = 0;
    std::streamoff bytes_written = 0;
    int const log_file_roll_size_bytes;
    std::string const name;
    std::unique_ptr<std::ofstream> os;

public:
    explicit file_writter(std::string log_directory, std::string log_file_name, int log_file_roll_size_mb)
      : log_file_roll_size_bytes(log_file_roll_size_mb * 1024 * 1024), name(log_directory + log_file_name) {
        roll_file();
    }

    void write(zipline& logline) {
        auto pos = os->tellp();
        logline.write_into(*os);
        bytes_written += os->tellp() - pos;
        if (bytes_written > log_file_roll_size_bytes) {
            roll_file();
        }
    }

private:
    void roll_file() {
        if (os) {
            os->flush();
            os->close();
        }

        bytes_written = 0;
        os.reset(new std::ofstream());
        std::string log_file_name = name;
        log_file_name.append(std::to_string(++file_number));
        log_file_name.append(".txt");
        os->open(log_file_name, std::ofstream::out | std::ofstream::trunc);
    }

};

// 自旋锁
struct spinlock {
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire));
    }
    void unlock() {
        flag.clear(std::memory_order_release); 
    }
private:
    std::atomic_flag flag;
};

// RALL锁
struct spinlock_guard {
    spinlock_guard(std::atomic_flag& flag) : flag(flag) {
        while (this->flag.test_and_set(std::memory_order_acquire));
    }
    ~spinlock_guard() {this->flag.clear(std::memory_order_release); }

private:
    std::atomic_flag& flag;
};


static constexpr const uint64_t const_buffer_sz = 1024*1024;

// 一片固定的约为1mb的缓冲区，用于减少内存碎片
template <typename T>
class const_buffer {
    struct Item {
        T val; 
        Item(T&& tmp) : val(std::move(tmp)) {}
        ~Item() = default;
    };
    const size_t sz = 0; // 1mb
    Item* buf = nullptr;
    std::unique_ptr<std::atomic<uint32_t>[]> write_state;
public:
    const_buffer() 
        : sz(const_buffer_sz/sizeof(Item))
        , buf(static_cast<Item*>(std::malloc(sz * sizeof(Item))))
        , write_state(new std::atomic<uint32_t>[sz+1]) {
        for (size_t i = 0; i <= sz; ++i) {
            write_state[i].store(0, std::memory_order_relaxed);
        }
    }
    ~const_buffer() {
        unsigned int write_count = write_state[sz].load();
        for (size_t i = 0; i < write_count; ++i) {
            buf[i].~Item();  // 手动解引用
        }
        std::free(buf);
    }

    /**
     * @brief 推任务，当缓冲满时返回true
     * @param t 
     * @param write_index 
     * @return 当缓冲满时返回true
     */
    bool push(T&& t, unsigned int const write_index) {
        new (&buf[write_index]) Item(std::move(t));
        write_state[write_index].store(1, std::memory_order_release);
        return write_state[sz].fetch_add(1, std::memory_order_acquire) + 1 == sz;
    }

    /**
     * @brief 尝试弹出
     * @param t 
     * @param read_index 
     * @return 成功: true 失败: false
     */
    bool try_pop(T& t, unsigned int const read_index) {
        if (write_state[read_index].load(std::memory_order_acquire)) {
            Item& item = buf[read_index];
            t = std::move(item.val);
            return true;
        }
        return false;
    }
    size_t get_size() {
        return sz;
    }

    const_buffer(const_buffer const&) = delete;
    const_buffer& operator=(const_buffer const&) = delete;

};

// 缓冲队列
class buffer_queue {
    using buffer = const_buffer<zipline>;
    size_t buf_size = 0;
public:
    buffer_queue() 
        : cur_rbuf{nullptr}
        , widx(0)
        , flag{ATOMIC_FLAG_INIT}
        , ridx(0) {
        setup_next_wbuf();
        buf_size = cur_wbuf.load()->get_size(); 
    }
    buffer_queue(buffer_queue const&) = delete;
    buffer_queue& operator=(buffer_queue const&) = delete;
    ~buffer_queue() = default;

    void push(zipline&& line) {
        unsigned int idx = widx.fetch_add(1, std::memory_order_relaxed);
        if (idx < buf_size) {
            if (cur_wbuf.load(std::memory_order_acquire)->push(std::move(line), idx)) {
                setup_next_wbuf();
            }
        } else {
            while (widx.load(std::memory_order_acquire) >= buf_size);
            push(std::move(line));
        }
    }
    bool try_pop(zipline& line)  {
        if (cur_rbuf == nullptr)
            cur_rbuf = get_next_rbuf();

        buffer* read_buffer = cur_rbuf;
        if (read_buffer == nullptr)
            return false;

        if (read_buffer->try_pop(line, ridx)) {
            ridx++;
            if (ridx == buf_size) {
                ridx = 0;
                cur_rbuf = nullptr;
                spinlock_guard lock(flag);
                buffers.pop();
            }
            return true;
        }

        return false;
    }

private:
    void setup_next_wbuf() {
        std::unique_ptr<buffer> next_wbuf(new buffer());
        cur_wbuf.store(next_wbuf.get(), std::memory_order_release);
        spinlock_guard lock(flag);
        buffers.push(std::move(next_wbuf));
        widx.store(0, std::memory_order_relaxed);
    }

    buffer* get_next_rbuf() {
        spinlock_guard lock(flag);
        return buffers.empty() ? nullptr : buffers.front().get();
    }

private:
    std::queue<std::unique_ptr<buffer>> buffers;
    std::atomic<buffer*> cur_wbuf;
    buffer* cur_rbuf;
    std::atomic<unsigned int> widx;
    std::atomic_flag flag;
    unsigned int ridx;
};

} // namespace details

// 异步日志
class asynclogger {
    details::file_writter writter;
    std::thread log_thread;
    details::buffer_queue mesg_que;

    bool stop = false;
    static std::unique_ptr<asynclogger> logger;
    static std::mutex obj_lock;
private:
    explicit asynclogger(const char* file_path, const char* file_name, int roll_sz = 1) 
        : writter(file_path, file_name, roll_sz) {
        log_thread = std::thread(&asynclogger::worker, this);
    }
    
public:
    /**
     * @brief 单例初始化日志系统
     * @param file_path 生成文件的路径 
     * @param file_name 生成文件的名字
     * @param roll_sz 日志文件最大值
     * @return 文件系统实例指针
     */
    static asynclogger* initialize(const char* file_path, const char* file_name, int roll_sz = 1) {
        if (!logger) {
            obj_lock.lock();
            if (!logger) {  // 1mb + 0.5mb + 0.5mb = 2mb
                logger.reset(new asynclogger(file_path, file_name, roll_sz));
                obj_lock.unlock();
                return logger.get();
            }
            obj_lock.unlock();
        }
        throw std::logic_error("Tried to init log system twice");
    }

    // 获取日志系统实例，若未初始化，则可能得到nullptr
    static asynclogger* instance() {
        auto obj = logger.get();
        return obj;
    }

    // 关闭异步日志系统
    void shutdown() {
        obj_lock.lock();
        if (!stop) {
            stop = true;
            obj_lock.unlock();
        }
        obj_lock.unlock();
    }


    // 辅助函数（输入日志行）
    bool operator == (details::zipline& line) {
        mesg_que.push(std::move(line));
        return true;
    }

    ~asynclogger() {
        shutdown();
        log_thread.join();
    }
private:
    void worker() {
        details::zipline line; 
        do {
            if (mesg_que.try_pop(line)) {
                writter.write(line);
            } else {
                std::this_thread::yield();
            }
        } while (!stop);

        // Guaranteed
        while (mesg_que.try_pop(line)) {
            writter.write(line);
        }
        
    }

};

} //nc::log

#define LOGGER (*nc::log::asynclogger::instance())
#define LOG_INFO LOGGER == nc::log::details::zipline(nc::log::details::LOG_LEVEL::INFO, __FILE__, __func__, __LINE__)
#define LOG_WARN LOGGER == nc::log::details::zipline(nc::log::details::LOG_LEVEL::WARN, __FILE__, __func__, __LINE__)
#define LOG_CRIT LOGGER == nc::log::details::zipline(nc::log::details::LOG_LEVEL::CRIT, __FILE__, __func__, __LINE__)