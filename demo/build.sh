#!bin/base

include="../include"
tar="echo/echo_client.cc"
# tar="echo/echo_server.cc"

src="../src/signal.cc"

g++ -std=c++11 -I $include $src $tar -o demo
