#include <cstring>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>

// 相同的计时机制
double get_absolute_time() {
    std::chrono::duration<double> timestamp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return timestamp.count();
}

// 测试本机memcpy的性能
int main(int argn, char** args) {
    assert(argn == 2);
    uint64_t count = atoi(args[1]);

    
    char buf1[16*1024]; // 16k
    char buf2[16*1024]; // 16k
    double start = get_absolute_time();
    for (int i = 0; i < count; ++i) {
        memcpy(buf1, buf2, 16*1024);
    }
    double end = get_absolute_time();
    double mb = (double)(count*16)/(double)1024;
    std::cout<<"Result: "<<mb/(end-start)<<" Mb/s"<<std::endl;
    std::cout<<"4次拷贝-2次读写(读写字节数都计算在内): "<<(mb/2)/(end-start)<<std::endl;

}