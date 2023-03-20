#!/bin/bash
# 你需要将spdlog克隆到本文件夹中再运行bench文件

# nclog bench
# g++ -O3 -std=c++11 -I ../../include ./nclog_bench.cpp ../../src/log.cc -lpthread -o nclog_bench

# nclog vs spdlog
g++ -O3 -std=c++11 -pthread ../src/log.cc nclog_vs_spdlog.cc -I ./spdlog/include -I ../include  -o nclog_vs_spdlog