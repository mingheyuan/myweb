CXX ?= g++
CXXFLAGS += -std=c++11 -Wall -Wextra -g

server:main.cpp config.cpp webserver.cpp
	$(CXX) $(CXXFLAGS) -o server $^

clean:
	rm -f server