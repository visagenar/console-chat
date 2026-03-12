CXX      = g++
CXXFLAGS = -std=c++11 -pthread
LDFLAGS  = -lws2_32
RM       = rm -f

TARGETS = server.exe client.exe

all: $(TARGETS)

server.exe: server.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

client.exe: client.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(TARGETS)

.PHONY: all clean
