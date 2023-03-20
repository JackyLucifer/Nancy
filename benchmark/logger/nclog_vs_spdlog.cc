// #include "NanoLog.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "nancy/log/log.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <vector>

/* Returns microseconds since epoch */
uint64_t get_absolute_time() { return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::microseconds(1); }

template <typename Function>
void run_log_benchmark(Function&& f, char const* const logger) {
    int iterations = 100000;
    std::vector<uint64_t> latencies;
    char const* const benchmark = "benchmark";
    for (int i = 0; i < iterations; ++i) {
        uint64_t begin = get_absolute_time();
        f(i, benchmark);
        uint64_t end = get_absolute_time();
        latencies.push_back(end - begin);
    }
    std::sort(latencies.begin(), latencies.end());
    uint64_t sum = 0;
    for (auto l : latencies) {
        sum += l;
    }
    printf("%s percentile latency numbers in microseconds\n%9s %9s %9s %9s %9s %9s %9s \n%9ld %9ld %9ld %9ld %9ld %9ld %9lf \n", logger,
           "50th", "75th", "90th", "99th", "99.9th", "Worst", "Average", latencies[(size_t)iterations * 0.5],
           latencies[(size_t)iterations * 0.75], latencies[(size_t)iterations * 0.9], latencies[(size_t)iterations * 0.99],
           latencies[(size_t)iterations * 0.999], latencies[latencies.size() - 1], (sum * 1.0) / latencies.size());
}

template <typename Function>
void run_benchmark(Function&& f, int thread_count, char const* const logger) {
    printf("\nThread count: %d\n", thread_count);
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(run_log_benchmark<Function>, std::ref(f), logger);
    }
    for (int i = 0; i < thread_count; ++i) {
        threads[i].join();
    }
}

void print_usage() {
    char const* const executable = "./nclog_vs_spdlog";
    printf("Usage \n1. %s nclog\n2. %s spdlog\n", executable, executable);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "nclog") == 0) {
        nc::log::asynclogger::initialize("/tmp/", "nclog", 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // wait for async thread created

        auto nanolog_benchmark = [](int i, char const* const cstr) { 
            LOG_INFO << "Logging " << cstr << i << 0 << 'K' << -42.42; 
        };
        for (auto threads : {1, 2, 3, 4})
            run_benchmark(nanolog_benchmark, threads, "nclog");

    } else if (strcmp(argv[1], "spdlog") == 0) {

        spdlog::init_thread_pool(100000, 1);
        auto spd_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "/tmp/async_log.txt");
        auto spdlog_benchmark = [&spd_logger](int i, char const* const cstr) {
            spd_logger->info("Logging {}{}{}{}{}", cstr, i, 0, 'K', -42.42);
        };
        for (auto threads : {1, 2, 3, 4})
            run_benchmark(spdlog_benchmark, threads, "spdlog");

    } else {
        print_usage();
    }

    return 0;
}
