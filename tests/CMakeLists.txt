cmake_minimum_required(VERSION 3.5)

project(tests)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG")  


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()  

add_library(signal
    STATIC
    ../src/signal.cc
)
target_include_directories(signal
    PRIVATE
    ../include
)
add_library(log
    STATIC
    ../src/log.cc
)
target_include_directories(log
    PRIVATE
    ../include
)

include_directories(../include)

# test_creactors
add_executable(test_creactors test_creactors.cc)
target_link_libraries(test_creactors PRIVATE signal)

# test_reactor
add_executable(test_reactor test_reactor.cc)
target_link_libraries(test_reactor PRIVATE signal)

# test_log
add_executable(test_log test_log.cc)
target_link_libraries(test_log PRIVATE log)

# test_memory
add_executable(test_memory test_memory.cc)

# test_timer_master
add_executable(test_timer_master test_timer_master.cc)

# test_timer
add_executable(test_timer test_timer.cc)









