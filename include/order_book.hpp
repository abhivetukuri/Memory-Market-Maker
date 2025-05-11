#pragma once

#include "types.hpp"
#include "memory_pool.hpp"
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>

namespace mm
{

    /**
     * Price level in the order book
     * Contains aggregated quantity at a specific price
     */
    struct alignas(ALIGNMENT) PriceLevel
    {
        Price price;
        Quantity total_quantity;
        uint32_t order_count;
        Timestamp last_update;

        PriceLevel() : price(0), total_quantity(0), order_count(0), last_update(0) {}
        PriceLevel(Price p, Quantity q) : price(p), total_quantity(q), order_count(1), last_update(get_timestamp()) {}
    };

    /**
     * Individual order in the order book
     */
    struct alignas(ALIGNMENT) Order
    {
        OrderId id;
        SymbolId symbol;
        Price price;
        Quantity quantity;
        Quantity filled_quantity;
        OrderSide side;
        OrderType type;
        OrderStatus status;
        Timestamp timestamp;
        PriceLevel *level; // Pointer to associated price level

        Order() : id(0), symbol(0), price(0), quantity(0), filled_quantity(0),
                  side(OrderSide::BUY), type(OrderType::LIMIT), status(OrderStatus::PENDING),
                  timestamp(0), level(nullptr) {}
    };

    /**
     * High-performance order book using memory pools
     * Supports microsecond quote updates with no heap allocations
     */
    class OrderBook
    {
    public:
        explicit OrderBook(SymbolId symbol);
        ~OrderBook() = default;

        // Non-copyable, non-movable
        OrderBook(const OrderBook &) = delete;
        OrderBook &operator=(const OrderBook &) = delete;
        OrderBook(OrderBook &&) = delete;
        OrderBook &operator=(OrderBook &&) = delete;

        /**
         * Add a new order to the book
         */
        bool add_order(OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type = OrderType::LIMIT);

        /**
         * Cancel an order
         */
        bool cancel_order(OrderId order_id, Quantity quantity = 0);

        /**
         * Modify an existing order
         */
        bool modify_order(OrderId order_id, Price new_price, Quantity new_quantity);

        /**
         * Execute a trade (fill orders)
         */
        bool execute_trade(Price price, Quantity quantity, OrderSide side);

        /**
         * Get best bid price and quantity
         */
        std::pair<Price, Quantity> get_best_bid() const;

        /**
         * Get best ask price and quantity
         */
        std::pair<Price, Quantity> get_best_ask() const;

        /**
         * Get mid price
         */
        Price get_mid_price() const;

        /**
         * Get spread
         */
        Price get_spread() const;

        /**
         * Get order book depth
         */
        std::vector<std::pair<Price, Quantity>> get_bids(size_t depth = MAX_ORDER_BOOK_DEPTH) const;
        std::vector<std::pair<Price, Quantity>> get_asks(size_t depth = MAX_ORDER_BOOK_DEPTH) const;

        /**
         * Get order by ID
         */
        const Order *get_order(OrderId order_id) const;

        /**
         * Get symbol ID
         */
        SymbolId get_symbol() const { return symbol_; }

        /**
         * Check if order book is empty
         */
        bool empty() const { return bids_.empty() && asks_.empty(); }

        /**
         * Get total number of orders
         */
        size_t order_count() const { return orders_.size(); }

        /**
         * Get total number of price levels
         */
        size_t level_count() const { return bids_.size() + asks_.size(); }

        /**
         * Get order book statistics
         */
        struct Stats
        {
            size_t total_orders;
            size_t active_orders;
            size_t bid_levels;
            size_t ask_levels;
            Price best_bid;
            Price best_ask;
            Price mid_price;
            Price spread;
        };

        Stats get_stats() const;

    private:
        SymbolId symbol_;

        // Price level maps (price -> PriceLevel*)
        std::map<Price, PriceLevel *, std::greater<Price>> bids_; // Descending for bids
        std::map<Price, PriceLevel *, std::less<Price>> asks_;    // Ascending for asks

        // Order storage
        std::map<OrderId, Order *> orders_;

        // Memory pools
        MemoryPool<Order> order_pool_;
        MemoryPool<PriceLevel> level_pool_;

        // Thread safety
        mutable std::mutex mutex_;

        /**
         * Find or create price level
         */
        PriceLevel *get_or_create_level(Price price, OrderSide side);

        /**
         * Remove price level if empty
         */
        void remove_empty_level(Price price, OrderSide side);

        /**
         * Update price level statistics
         */
        void update_level_stats(PriceLevel *level, Quantity delta, bool add_order);

        /**
         * Find order by ID
         */
        Order *find_order(OrderId order_id);

        /**
         * Remove order from price level
         */
        void remove_order_from_level(Order *order);
    };

    /**
     * Order book manager for multiple symbols
     */
    class OrderBookManager
    {
    public:
        OrderBookManager();
        ~OrderBookManager() = default;

        // Non-copyable, non-movable
        OrderBookManager(const OrderBookManager &) = delete;
        OrderBookManager &operator=(const OrderBookManager &) = delete;
        OrderBookManager(OrderBookManager &&) = delete;
        OrderBookManager &operator=(OrderBookManager &&) = delete;

        /**
         * Get or create order book for symbol
         */
        OrderBook *get_order_book(SymbolId symbol);

        /**
         * Add order to specific symbol
         */
        bool add_order(SymbolId symbol, OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type = OrderType::LIMIT);

        /**
         * Cancel order
         */
        bool cancel_order(SymbolId symbol, OrderId order_id, Quantity quantity = 0);

        /**
         * Modify order
         */
        bool modify_order(SymbolId symbol, OrderId order_id, Price new_price, Quantity new_quantity);

        /**
         * Execute trade
         */
        bool execute_trade(SymbolId symbol, Price price, Quantity quantity, OrderSide side);

        /**
         * Get order book for symbol
         */
        const OrderBook *get_order_book(SymbolId symbol) const;

        /**
         * Get all active symbols
         */
        std::vector<SymbolId> get_active_symbols() const;

        /**
         * Get total number of order books
         */
        size_t order_book_count() const { return order_books_.size(); }

    private:
        std::map<SymbolId, std::unique_ptr<OrderBook>> order_books_;
        mutable std::mutex mutex_;
    };

} // namespace mm