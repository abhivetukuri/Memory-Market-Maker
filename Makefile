CXX = g++
CXXFLAGS = -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Iinclude
DEBUGFLAGS = -std=c++20 -O0 -g -Wall -Wextra -Iinclude

# Source files
SOURCES = src/main.cpp src/order_book.cpp src/position_tracker.cpp \
          src/memory_pool.cpp src/strategy.cpp src/itch_parser.cpp \
          src/scenario_runner.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable
TARGET = memory_market_maker

# Test targets
TEST_ORDER_BOOK = test_order_book
TEST_POSITION_TRACKER = test_position_tracker
TEST_MEMORY_POOL = test_memory_pool
TEST_DATA_PROCESSING = test_data_processing

# Default target
all: $(TARGET)

# Debug build
debug: CXXFLAGS = $(DEBUGFLAGS)
debug: $(TARGET)

# Main executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) -lpthread

# Test executables
test: $(TEST_ORDER_BOOK) $(TEST_POSITION_TRACKER) $(TEST_MEMORY_POOL) $(TEST_DATA_PROCESSING)

$(TEST_ORDER_BOOK): tests/test_order_book.o src/order_book.o src/memory_pool.o
	$(CXX) tests/test_order_book.o src/order_book.o src/memory_pool.o -o $(TEST_ORDER_BOOK) -lpthread

$(TEST_POSITION_TRACKER): tests/test_position_tracker.o src/position_tracker.o
	$(CXX) tests/test_position_tracker.o src/position_tracker.o -o $(TEST_POSITION_TRACKER) -lpthread

$(TEST_MEMORY_POOL): tests/test_memory_pool.o src/memory_pool.o
	$(CXX) tests/test_memory_pool.o src/memory_pool.o -o $(TEST_MEMORY_POOL) -lpthread

$(TEST_DATA_PROCESSING): tests/test_data_processing.o src/order_book.o src/position_tracker.o src/memory_pool.o src/itch_parser.o src/scenario_runner.o
	$(CXX) tests/test_data_processing.o src/order_book.o src/position_tracker.o src/memory_pool.o src/itch_parser.o src/scenario_runner.o -o $(TEST_DATA_PROCESSING) -lpthread

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) tests/*.o $(TARGET) $(TEST_ORDER_BOOK) $(TEST_POSITION_TRACKER) $(TEST_MEMORY_POOL) $(TEST_DATA_PROCESSING)

# Run tests
run-tests: test
	./$(TEST_ORDER_BOOK)
	./$(TEST_POSITION_TRACKER)
	./$(TEST_MEMORY_POOL)
	./$(TEST_DATA_PROCESSING)

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
src/main.o: include/order_book.hpp include/position_tracker.hpp include/itch_parser.hpp include/scenario_runner.hpp include/types.hpp
src/itch_parser.o: include/itch_parser.hpp include/order_book.hpp include/position_tracker.hpp include/types.hpp
src/scenario_runner.o: include/scenario_runner.hpp include/order_book.hpp include/position_tracker.hpp include/types.hpp

.PHONY: all debug test clean run-tests run install uninstall 