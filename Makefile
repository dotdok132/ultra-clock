CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Isrc -Isrc/stb $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lpthread

SRCS = src/main.cpp src/ntp_client.cpp src/precise_clock.cpp src/renderer.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = ultra_clock

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESKTOPDIR ?= /usr/share/applications

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(DESKTOPDIR)
	install -m 644 ultra-clock.desktop $(DESTDIR)$(DESKTOPDIR)/ultra-clock.desktop
	@echo "UltraClock successfully installed to $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(DESKTOPDIR)/ultra-clock.desktop
	@echo "UltraClock successfully uninstalled."

.PHONY: all clean install uninstall
