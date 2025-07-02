#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <chrono>

namespace mm
{

    using SymbolId = uint16_t;
    using OrderId = uint64_t;
    using Price = int64_t;
    using Quantity = uint32_t;
    using Timestamp = uint64_t;
    using PositionSize = int64_t;
    using PnL = int64_t;

    constexpr size_t MAX_SYMBOLS = 10000;
    constexpr size_t MAX_ORDERS = 1000000;
    constexpr size_t MAX_LEVELS_PER_SIDE = 1000;
    constexpr size_t MAX_ORDER_BOOK_DEPTH = 50;

    constexpr Price PRICE_TICK_SIZE = 1;
    constexpr Price MAX_PRICE = std::numeric_limits<Price>::max() / 2;
    constexpr Price MIN_PRICE = -MAX_PRICE;

    constexpr size_t CACHE_LINE_SIZE = 64;
    constexpr size_t ALIGNMENT = CACHE_LINE_SIZE;

    enum class OrderSide : uint8_t
    {
        BUY = 0,
        SELL = 1
    };

    enum class OrderType : uint8_t
    {
        MARKET = 0,
        LIMIT = 1,
        STOP = 2
    };

    enum class OrderStatus : uint8_t
    {
        PENDING = 0,
        ACTIVE = 1,
        FILLED = 2,
        CANCELLED = 3,
        REJECTED = 4
    };

    enum class StrategyType : uint8_t
    {
        BASIC_SPREAD = 0,
        ADAPTIVE_SPREAD = 1,
        MEAN_REVERSION = 2,
        MOMENTUM = 3
    };

    struct PerformanceMetrics
    {
        Timestamp timestamp;
        uint64_t orders_processed;
        uint64_t quotes_generated;
        uint64_t fills_received;
        double avg_latency_ns;
        double max_latency_ns;
        double throughput_ops_per_sec;
    };

    inline constexpr Price price_from_dollars(double dollars)
    {
        return static_cast<Price>(dollars * 10000.0);
    }

    inline constexpr double price_to_dollars(Price price)
    {
        return static_cast<double>(price) / 10000.0;
    }

    inline Timestamp get_timestamp()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    }

    struct PoolStats
    {
        size_t total_allocated;
        size_t total_freed;
        size_t current_usage;
        size_t peak_usage;
        size_t allocation_count;
        size_t free_count;
    };

} // namespace mm