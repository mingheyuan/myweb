CXX ?= g++
CXXFLAGS += -std=c++11 -Wall -Wextra -g
LDFLAGS += -pthread

server:main.cpp config.cpp webserver.cpp http_conn.cpp threadpool.cpp timer.cpp logger.cpp
	$(CXX) $(CXXFLAGS) -o server $^ $(LDFLAGS)

clean:
	rm -f server