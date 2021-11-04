CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
LDFLAGS =

.PHONY: all clean

all: serwer

serwer: serwer.o err.o parser.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lstdc++fs

err.o: err.cpp err.h
	$(CXX) $(CXXFLAGS) -c $<
	
parser.o: parser.cpp parser.h
	$(CXX) $(CXXFLAGS) -c $<

main.o: main.cpp err.h parser.h
	$(CXX) $(CXXFLAGS) -c $<

.PHONY: clean
clean:
	rm *.o serwer

