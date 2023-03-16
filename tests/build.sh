
# tar=test_timer_master.cc
# tar=test_memory.cc
# tar=test_reactor.cc
# tar=test_tcp_server.cc
# tar=test_timer.cc
tar=test_log.cc
# tar=test_memory_quick_alloc.cc

include=../include

lib=
lib=-lpthread

# src=
src=../src/log.cc
# src=../src/signal.cc

g++ -std=c++11 -Wall -Werror -I ${include} $tar $src $lib -o test && ./test









