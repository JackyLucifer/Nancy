#!/bin/bash

g++ -std=c++11 -O3 -Wall -Werror -I ../../include client.cc ../../src/signal.cc -lpthread -o client
g++ -std=c++11 -O3 -Wall -Werror -I ../../include server.cc ../../src/signal.cc -lpthread -o server