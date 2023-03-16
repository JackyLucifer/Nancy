#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <cassert>
#include <memory>
#include <vector>
#include <iostream>
                                                                                                                        
namespace nc::base {  

/**
 * @brief 内存单元
 * @note 返回无类型内存地址
 * @note 内存安全
*/
class memory_unit {
    size_t sz_ = 0; 
    std::unique_ptr<char[]> addr_ = {nullptr};
public:
    memory_unit() = default;
    memory_unit(void* addr, size_t sz)
        : sz_(sz), addr_(static_cast<char*>(addr)) {}

    memory_unit(memory_unit&& other) = default;
    memory_unit(const memory_unit&) = delete;
    memory_unit& operator = (const memory_unit& other) = delete;
    memory_unit& operator = (memory_unit&& other)  {
        sz_ = other.sz_;
        other.sz_ = 0;
        addr_ = std::move(other.addr_);
        return *this;
    }

    ~memory_unit() = default;

public:
    void swap(memory_unit& other) {
        std::swap(sz_, other.sz_);
        addr_ = std::move(other.addr_);
    }

    // 该内存单元是否可用
    bool good() const noexcept {
        return static_cast<bool>(addr_);
    }
    // 获取内部指针
    void* get() noexcept {
        return static_cast<void*>(addr_.get());
    }
    // 获取尺寸
    size_t size() const noexcept {
        return sz_;
    }
    // 释放类对内存的管理并返还内存地址
    void* release() {
        return static_cast<void*>(addr_.release());
    }
};

struct memorys_base {
    memorys_base() = default;
    virtual ~memorys_base() = default;
    virtual memory_unit get(size_t sz) = 0;  // 获取
    virtual void recycle(memory_unit&&) = 0; // 回收  
};

/**
 * @brief 基于哈希表的轻量级内存池
 * @note 在获取完内存后需要归还，否则会使得内存池内存越来越少!
 * @note 我们设内存单元数为n，尺寸类型数为1，则所有接口的时间复杂度都为 O(1)
 * @note 关于内部数据结构本身带来的内存碎片问题：
 * 内部数据结构节点的大小是固定的，因此在维持内存池大小大致不变或者持续扩充的情况下，所产生的内存碎片基本都会被重复利用。
 * 你甚至可以在同一进程中申请多个内存池，因为归根结底他们的节点都是一样大小的。
*/
class memorys: public memorys_base {

    std::map<size_t, int> info_;  // 信息表, 用于排序和获取内存信息; 结构:<尺寸，数量>
    std::unordered_multimap<size_t, std::unique_ptr<char[]>> mems_; // 抽象内存单元的映射; 结构:<尺寸，内存单元>
public:
    using memory_info_list = std::vector<std::array<size_t, 2>>; // {nums, size}
    memorys() = default;
    memorys(const memory_info_list& info_list) {
        for (auto& each: info_list) {
            this->info_.emplace(each[1], each[0]); //转换
        }
        for (auto& each: this->info_) {
            for (int i = 0; i < each.second; ++i) {
                mems_.emplace(each.first, new char[each.first]);  
            }
        }
    }   
    memorys(std::initializer_list<std::array<size_t, 2>> info_list) {
        for (auto& each: info_list) {
            this->info_.emplace(each[1], each[0]); //转换
        }
        for (auto& each: this->info_) {
            for (int i = 0; i < each.second; ++i) {
                mems_.emplace(each.first, new char[each.first]);  
            }
        }
    }
    memorys(memorys&& other) noexcept = default;
    memorys(const memorys&) = delete;
    ~memorys() {}
public:
    /**
     * @brief 获取内存单元，如果没有匹配到内存的话返回空内存单元
     * @param sz 需要最少的内存空间
     * @return 内存单元
     */
    memory_unit get(size_t sz) override {
        auto info_it = info_.lower_bound(sz);
        if (info_it == info_.end()) { // 超过最大值
            return memory_unit();
        } else {
            auto keep_best = info_it->first;
            while (!info_it->second) { 
                info_it = info_.lower_bound(info_it->first+1); // 继续找
                if (info_it == info_.end()) {
                    return memory_unit(nullptr, keep_best);  // valid demand
                }
            }
            auto mem_it = mems_.find(info_it->first);
            memory_unit unit(static_cast<void*>(mem_it->second.release()), mem_it->first);

            info_it->second--;
            mems_.erase(mem_it);
            // std::cout<<mems_.count(128)<<std::endl;
            return unit;
        }
    }


    /**
     * @brief 添加内存段
     * @param addr 内存段
     * @param sz 内存段尺寸
     */
    void add(void* mem_addr, size_t sz) {
        info_[sz]++;
        mems_.emplace(sz, static_cast<char*>(mem_addr));
    }

    void recycle(memory_unit&& mem) override {
        info_[mem.size()]++;
        mems_.emplace(mem.size(), static_cast<char*>(mem.release()));
    }

    /**
     * @brief 获取尺寸为sz的内存单元的数量
     * @param sz 内存单元大小
     * @return 数量
     */
    int count_unit(size_t sz) {
        auto it = info_.find(sz);
        return (it == info_.end()) ? 0 : it->second;
    }

    /**
     * @brief 动态计算剩余的字节空间
     * @note 该调用有字节溢出风险
     */
    uint64_t count_bytes() {
        uint64_t res = 0;
        for (auto& each: info_) {
            res += static_cast<uint64_t>(each.first*each.second);
        }
        return res;
    }
};


/**
 * @brief 基于vector的改良内存池，只支持固定的内存单元类型
 * @note 
*/
class simple_memorys: public memorys_base {
    const size_t sz_;
    int cur_ = 0;
    std::vector<std::unique_ptr<char[]>> mems_;
public:
    simple_memorys() = default;
    explicit simple_memorys(int nums, size_t size)
      : sz_(size)
      , cur_(nums-1) {
        for (int i = 0; i < nums; ++i) {
            mems_.emplace_back(new char[sz_]);
        }
    }
    simple_memorys& operator = (simple_memorys&&) = default; 
    ~simple_memorys() = default;
public:
    // 获取固定类型的内存单元
    memory_unit get(size_t size) override {
        if (size > sz_) {
            return memory_unit(); // invalid demand
        } 
        if (cur_ > -1) {
            return memory_unit(static_cast<void*>(mems_[cur_--].release()), sz_);
        } else {  
            return memory_unit(nullptr, sz_); // valid demand
        }
    }

    /**
     * @brief 添加内存，不一定会扩充内存池
     * @param nums 固定的内存的数量
     */
    void add(int nums) {
        for (int i = 0; i < nums; ++i) {
            if (++cur_ < (int)mems_.size()) {
                mems_[cur_].reset(new char[sz_]);
            } else {
                mems_.emplace_back(new char[sz_]);
            }
        }
    }

    /**
     * @brief 回收内存单元
     * @param mem 内存单元: 其内存大小需与内存池大小一致
     * @note 对于内存单元的类型不做检查
     */
    void recycle(memory_unit&& mem) {
        if (++cur_ < (int)mems_.size()) {
            mems_[cur_].reset(static_cast<char*>(mem.release()));
        } else {
            mems_.emplace_back(static_cast<char*>(mem.release()));
        }

    }

    int count_unit() {
        return cur_+1;
    }

    size_t count_bytes() {
        return (size_t)(cur_+1) * sz_;
    }


};


} // namespace nc::base