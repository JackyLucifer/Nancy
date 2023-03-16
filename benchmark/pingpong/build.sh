g++ -std=c++11 -O3 -I ../../include server_A.cc ../../src/signal.cc -o serv_A
g++ -std=c++11 -O3 -I ../../include server_B.cc ../../src/signal.cc -lpthread -o serv_B