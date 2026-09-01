CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Isrc $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lpthread

SRCS = src/main.cpp src/ntp_client.cpp src/precise_clock.cpp src/renderer.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = ultra_clock

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
