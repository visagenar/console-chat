CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread
LDFLAGS = -lws2_32 -static -mconsole

all: server client

server: server.cpp common.h
	$(CXX) $(CXXFLAGS) server.cpp -o server.exe $(LDFLAGS)

client: client.cpp common.h
	$(CXX) $(CXXFLAGS) client.cpp -o client.exe $(LDFLAGS)

clean:
	rm -f *.exe

.PHONY: all clean