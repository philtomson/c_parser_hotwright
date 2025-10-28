# Makefile for Hotstate Machine Simulator

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude -Ithird_party
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/hotstate_sim

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install (copy to system path)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Test with example files
test: $(TARGET)
	@echo "Testing with example files..."
	$(TARGET) --base examples/basic_test/test --stimulus examples/basic_test/stimulus.txt --max-cycles 100

# Debug build
debug: CXXFLAGS += -DDEBUG -O0
debug: $(TARGET)

# Release build
release: CXXFLAGS += -DNDEBUG -O3
release: clean $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build the simulator (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  test      - Run basic tests"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release version"
	@echo "  install   - Install to system path"
	@echo "  help      - Show this help message"

# Dependencies
$(OBJDIR)/utils.o: include/utils.h
$(OBJDIR)/memory_loader.o: include/memory_loader.h include/utils.h

.PHONY: all clean test debug release install help directories