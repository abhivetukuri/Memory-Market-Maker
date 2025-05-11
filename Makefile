CXX = g++
CXXFLAGS = -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Iinclude
DEBUGFLAGS = -std=c++20 -O0 -g -Wall -Wextra -Iinclude

# Source files
SOURCES = src/main.cpp src/order_book.cpp src/position_tracker.cpp \
          src/memory_pool.cpp src/mmap_manager.cpp src/market_maker.cpp \
          src/strategy.cpp src/pl_calculator.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable
TARGET = memory_market_maker

# Test sources
TEST_SOURCES = tests/test_order_book.cpp tests/test_position_tracker.cpp tests/test_memory_pool.cpp
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
TEST_TARGET = memory_market_maker_tests

# Default target
all: $(TARGET)

# Debug build
debug: CXXFLAGS = $(DEBUGFLAGS)
debug: $(TARGET)

# Main executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) -lpthread

# Test executable
test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) src/order_book.o src/position_tracker.o src/memory_pool.o
	$(CXX) $(TEST_OBJECTS) src/order_book.o src/position_tracker.o src/memory_pool.o -o $(TEST_TARGET) -lpthread

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)

# Run tests
run-tests: test
	./$(TEST_TARGET)

# Run main program
run: $(TARGET)
	./$(TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Dependencies
src/order_book.o: include/order_book.hpp include/memory_pool.hpp include/types.hpp
src/position_tracker.o: include/position_tracker.hpp include/types.hpp
src/main.o: include/order_book.hpp include/position_tracker.hpp include/types.hpp

.PHONY: all debug test clean run-tests run install uninstall 