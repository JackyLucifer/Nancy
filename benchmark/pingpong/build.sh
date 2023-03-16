g++ -std=c++11 -O3 -I ../../include server_A.cc ../../src/signal.cc -o server
g++ -std=c++11 -O3 -I ../../include client_B.cc ../../src/signal.cc -lpthread -o client