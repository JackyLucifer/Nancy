g++ -std=c++11 -O3 -I ../../include server.cc ../../src/signal.cc -o server
g++ -std=c++11 -O3 -I ../../include client.cc ../../src/signal.cc -lpthread -o client