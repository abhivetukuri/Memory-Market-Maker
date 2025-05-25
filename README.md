# Memory Market Maker

A high-performance market making system built in C++20 using memory-mapped files, fixed-size memory pools, and microsecond quote updates.

## Features

- **Memory-Aggressive Design**: Uses memory pools and pre-allocated structures to eliminate heap allocations
- **Microsecond Performance**: Achieves sub-microsecond quote updates through lock-free operations and cache-aligned data structures
- **Real-time P&L Calculation**: Tracks realized and unrealized P&L with position limits and risk management
- **Memory-Mapped Persistence**: Uses mmap for efficient position and order book persistence
- **Configurable Strategies**: Framework for implementing various market making strategies
- **Multi-Symbol Support**: Handles multiple symbols with independent order books and position tracking

## Architecture

### Core Components

1. **OrderBook**: High-performance order book with memory pool allocation
2. **PositionTracker**: Real-time position and P&L tracking with risk limits
3. **MemoryPool**: Fixed-size object allocation for microsecond performance
4. **OrderBookManager**: Multi-symbol order book management

### Key Design Principles

- **Zero Heap Allocations**: All critical paths use pre-allocated memory pools
- **Cache-Aligned Data**: Structures aligned to cache line boundaries for optimal performance
- **Lock-Free Operations**: Minimize contention with fine-grained locking
- **Memory-Mapped Files**: Efficient persistence without serialization overhead

## Building

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20+
- POSIX-compliant system (Linux/macOS)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Running

```bash
# Run the main program with benchmarks
./memory_market_maker

# Run tests
make test
```

## Performance

The system is designed for microsecond quote updates:

- **Order Book Operations**: < 1 microsecond per order
- **Position Updates**: < 1 microsecond per trade
- **Memory Pool Allocation**: < 100 nanoseconds per allocation
- **Throughput**: > 1M operations/second on modern hardware

## Usage Example

```cpp
#include "order_book.hpp"
#include "position_tracker.hpp"

using namespace mm;

// Create order book and position tracker
OrderBook order_book(1);  // Symbol 1
PositionLimits limits;
limits.max_position_size = 10000;
PositionTracker tracker(limits);

// Place market making orders
order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY);
order_book.add_order(2, price_from_dollars(100.10), 1000, OrderSide::SELL);

// Execute trades
bool executed = order_book.execute_trade(price_from_dollars(100.00), 500, OrderSide::SELL);
if (executed) {
    tracker.record_trade(1, price_from_dollars(100.00), 500, OrderSide::BUY, 1);
}

// Get current state
auto [bid_price, bid_qty] = order_book.get_best_bid();
auto [ask_price, ask_qty] = order_book.get_best_ask();
PnL total_pnl = tracker.get_total_pnl();
```

## Data Structures

### Order Book

- **PriceLevel**: Aggregated quantity at a specific price
- **Order**: Individual order with metadata and status
- **Memory Pool**: Pre-allocated pools for orders and price levels

### Position Tracking

- **Position**: Long/short quantities with average prices
- **Trade**: Historical trade records for P&L calculation
- **PositionLimits**: Risk management limits and constraints

### Memory Management

- **MemoryPool**: Template-based fixed-size allocation
- **ThreadLocalPool**: Lock-free thread-local allocation
- **MMapPositionTracker**: Memory-mapped position persistence

## Configuration

### Position Limits

```cpp
PositionLimits limits;
limits.max_position_size = 100000;    // Maximum total position size
limits.max_long_position = 50000;     // Maximum long position
limits.max_short_position = 50000;    // Maximum short position
limits.max_daily_loss = 1000000;      // Maximum daily loss (in ticks)
limits.max_drawdown = 500000;         // Maximum drawdown (in ticks)
```

### Performance Tuning

- Adjust memory pool sizes based on expected order volume
- Configure cache line alignment for your target architecture
- Tune thread pool sizes for your hardware

## Testing

The project includes comprehensive testing capabilities for both unit tests and real market data processing:

### Unit Tests

```bash
# Run all unit tests
make test
./memory_market_maker_tests

# Run individual test files
make tests/test_order_book.o
make tests/test_position_tracker.o
make tests/test_memory_pool.o
make tests/test_data_processing.o
```

### Data Processing Tests

The project now supports testing with real market data:

#### ITCH Data Processing
- **NASDAQ TotalView-ITCH format** support for real market data
- High-performance binary message parsing
- Support for all major ITCH message types (orders, executions, cancels, trades)
- Automatic symbol mapping and price conversion
- Performance statistics and throughput measurement

#### Scenario Testing
- **Systematic testing** using scenario files in `data/matching/`
- Support for complex order book scenarios
- Market order execution with slippage
- Position tracking and P&L calculation
- Performance benchmarking

### Test Runner Script

Use the provided test runner script for easy testing:

```bash
# Run all tests
./run_tests.sh

# Run specific test types
./run_tests.sh build      # Build only
./run_tests.sh unit       # Unit tests only
./run_tests.sh main       # Main program tests
./run_tests.sh itch       # ITCH data processing
./run_tests.sh scenarios  # Scenario testing
./run_tests.sh performance # Performance benchmarks
```

### Data Files

The project expects the following data files for testing:

- `data/sample.itch` - NASDAQ ITCH market data file
- `data/matching/` - Directory containing scenario files (scenario-01.txt, scenario-02.txt, etc.)

If these files are not present, the corresponding tests will be skipped with appropriate warnings.

## Future Enhancements

- **Market Making Strategies**: Implement adaptive spread, mean reversion, and momentum strategies
- **Network Layer**: Add low-latency market data and order routing
- **Risk Management**: Advanced position sizing and correlation analysis
- **Backtesting**: Historical simulation and strategy validation
- **Monitoring**: Real-time performance metrics and alerting

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes with appropriate tests
4. Ensure all tests pass
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Inspired by high-frequency trading systems and market making literature
- Uses modern C++20 features for optimal performance
- Built with microsecond latency requirements in mind