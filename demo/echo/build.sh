#!bin/base

include="../../include"
g++ -std=c++11 -I $include ./echo_server.cc ../../src/signal.cc -o echo_server
g++ -std=c++11 -I $include ./echo_client.cc ../../src/signal.cc -o echo_client
