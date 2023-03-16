#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <set>

namespace nc::base {
namespace _base {

// 存储时间
using time_unit = uint64_t;

// 时间精度
template <class T>
struct duration {
    typedef std::chrono::duration<time_unit, T> type;
};
template <>
struct duration<void> {
    typedef std::chrono::duration<time_unit> type;
};

}  // namespace _nc

template <typename ratio_t>
struct timer_master_base;


/**
 * @brief  最高精度的定时器，注意超时回调是非必须的，可以后续绑定
 * @tparam ratio_t 精度，默认为秒(seconds)，可替换为std::milli/std::micro/std::nano。
 */
template <typename ratio_t = void>
class timer {
public:
    using clock_t = std::chrono::steady_clock;
    using callback_t = typename std::function<void()>;
    using duration_t = typename _base::duration<ratio_t>::type;
    using timestamp_t = std::chrono::time_point<clock_t, duration_t>;

private:
    callback_t timeout_cb;      // 超时回调
    timestamp_t timeout_stamp;  // 超时时间点

public:
    timer(int timeout)
      : timeout_stamp(std::chrono::time_point_cast<duration_t>(clock_t::now() + duration_t(timeout))) {
    }
    timer(int timeout, const clock_t::time_point& begin)
      : timeout_stamp(std::chrono::time_point_cast<duration_t>(begin + duration_t(timeout))) {
    }
    timer(const timestamp_t& timeout_stamp)
      : timeout_stamp(timeout_stamp) {
    }
    timer(const timer& other) = default;
    timer& operator=(const timer& other) = default;

public:
    // 绑定到管理器上
    void attach(timer_master_base<ratio_t>& master) {
        master.bind(*this);
    }
    // 解除绑定
    void detach(timer_master_base<ratio_t>& master) {
        master.release(*this);
    }
    // 绑定到管理器上
    void attach(timer_master_base<ratio_t>* master) {
        master->bind(*this);
    }
    // 解除绑定
    void detach(timer_master_base<ratio_t>* master) {
        master->release(*this);
    }

    // 绑定一个超时回调函数
    template <typename F, typename... Args>
    void bind(F&& cb, Args&&... args) {
        timeout_cb = std::bind(std::forward<F>(cb), std::forward<Args>(args)...);
    }
    // 重新绑定
    template <typename F, typename... Args>
    void rebind(F&& cb, Args&&... args) {
        timeout_cb = std::bind(std::forward<F>(cb), std::forward<Args>(args)...);
    }
    // 重置
    void reset(const timer<ratio_t>& temp) {
        *this = temp;
    }
    // 检查是否超时
    bool check() const {
        return std::chrono::time_point_cast<duration_t>(clock_t::now()) > timeout_stamp;
    }
    // 检查是否超时
    bool check(timestamp_t now) const {
        return now > timeout_stamp;
    }
    // 检查是否超时
    bool check(clock_t::time_point now) const {
        return std::chrono::time_point_cast<duration_t>(now) > timeout_stamp;
    }

public:
    // 执行回调
    void operator()() const {
        if (static_cast<bool>(timeout_cb)) {
            timeout_cb();
        }
    }
    bool operator<(const timer<ratio_t>& other) const {
        return this->timeout_stamp < other.timeout_stamp;
    }
};

/**
 * @brief 定时器基类
 * @tparam ratio_t
 */
template <typename ratio_t = void>
struct timer_master_base {
    // 析构
    virtual ~timer_master_base() = default;
    // 添加定时器
    virtual void bind(const timer<ratio_t>& t) = 0;
    // 删除定时器
    virtual void release(const timer<ratio_t>& t) = 0;
};


template <typename ratio_t = void>
class timer_master : public timer_master_base<ratio_t> {
    using container_t = typename std::multiset<timer<ratio_t>>;
    container_t timers;

public:
    // 绑定
    void bind(const timer<ratio_t>& t) override {
        timers.emplace(t);
    }
    // 删除
    void release(const timer<ratio_t>& t) override {
        timers.erase(t);
    }
    // 尺寸
    template <typename size_type = decltype(timers.size())>
    size_type size() {
        return timers.size();
    }
    // 清理超时定时器并执行回调
    void clean_timeout() {
        timer<ratio_t> temp(0);  // 瞬间过期
        auto bound = timers.lower_bound(temp);
        for (auto it = timers.begin(); it != bound; ++it) {
            (*it)();  // 执行回调
        }
        timers.erase(timers.begin(), bound);
    }
};

}  // namespace nc::base
