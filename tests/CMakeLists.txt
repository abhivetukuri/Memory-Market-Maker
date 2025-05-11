# Test executable
add_executable(memory_market_maker_tests
    test_order_book.cpp
    test_position_tracker.cpp
    test_memory_pool.cpp
)

# Link with main library
target_link_libraries(memory_market_maker_tests
    memory_market_maker
)

# Add tests
add_test(NAME OrderBookTest COMMAND memory_market_maker_tests --gtest_filter=OrderBookTest.*)
add_test(NAME PositionTrackerTest COMMAND memory_market_maker_tests --gtest_filter=PositionTrackerTest.*)
add_test(NAME MemoryPoolTest COMMAND memory_market_maker_tests --gtest_filter=MemoryPoolTest.*) 