BINARY = myPTA
CXX = g++
CXXFLAGS=-g --std=c++17 -Wall -Wextra
SRCS = $(shell find src/ -name "*.cpp")
HEADERS = $(shell find src/ -name "*.hpp")
OBJS = $(SRCS:.c=.o)

all: $(BINARY)

clean:
	rm $(BINARY)

$(BINARY): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LIBS) $(OBJS) $(HEADERS) -o $(BINARY)
