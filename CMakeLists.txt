cmake_minimum_required(VERSION 3.20)
project(MemoryMarketMaker VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Wpedantic")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/order_book.cpp
    src/position_tracker.cpp
    src/market_maker.cpp
    src/memory_pool.cpp
    src/mmap_manager.cpp
    src/strategy.cpp
    src/pl_calculator.cpp
)

# Create executable
add_executable(memory_market_maker ${SOURCES})

# Link libraries
target_link_libraries(memory_market_maker 
    Threads::Threads
)

# Install target
install(TARGETS memory_market_maker DESTINATION bin)

# Add tests
enable_testing()
add_subdirectory(tests) 