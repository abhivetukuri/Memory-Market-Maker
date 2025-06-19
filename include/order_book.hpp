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

    struct alignas(ALIGNMENT) PriceLevel
    {
        Price price;
        Quantity total_quantity;
        uint32_t order_count;
        Timestamp last_update;

        PriceLevel() : price(0), total_quantity(0), order_count(0), last_update(0) {}
        PriceLevel(Price p, Quantity q) : price(p), total_quantity(q), order_count(1), last_update(get_timestamp()) {}
    };

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
        PriceLevel *level;

        Order() : id(0), symbol(0), price(0), quantity(0), filled_quantity(0),
                  side(OrderSide::BUY), type(OrderType::LIMIT), status(OrderStatus::PENDING),
                  timestamp(0), level(nullptr) {}
    };

    // Supports microsecond quote updates with no heap allocations
    class OrderBook
    {
    public:
        explicit OrderBook(SymbolId symbol);
        ~OrderBook() = default;

        OrderBook(const OrderBook &) = delete;
        OrderBook &operator=(const OrderBook &) = delete;
        OrderBook(OrderBook &&) = delete;
        OrderBook &operator=(OrderBook &&) = delete;

        bool add_order(OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type = OrderType::LIMIT);
        bool cancel_order(OrderId order_id, Quantity quantity = 0);
        bool modify_order(OrderId order_id, Price new_price, Quantity new_quantity);
        bool execute_trade(Price price, Quantity quantity, OrderSide side);
        std::pair<Price, Quantity> get_best_bid() const;
        std::pair<Price, Quantity> get_best_ask() const;
        Price get_mid_price() const;
        Price get_spread() const;
        std::vector<std::pair<Price, Quantity>> get_bids(size_t depth = MAX_ORDER_BOOK_DEPTH) const;
        std::vector<std::pair<Price, Quantity>> get_asks(size_t depth = MAX_ORDER_BOOK_DEPTH) const;
        const Order *get_order(OrderId order_id) const;
        SymbolId get_symbol() const { return symbol_; }
        bool empty() const { return bids_.empty() && asks_.empty(); }
        size_t order_count() const { return orders_.size(); }
        size_t level_count() const { return bids_.size() + asks_.size(); }
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

        std::map<Price, PriceLevel *, std::greater<Price>> bids_; // Descending for bids
        std::map<Price, PriceLevel *, std::less<Price>> asks_; // Ascending for asks
        std::map<OrderId, Order *> orders_;
        MemoryPool<Order> order_pool_;
        MemoryPool<PriceLevel> level_pool_;
        mutable std::mutex mutex_;

        PriceLevel *get_or_create_level(Price price, OrderSide side);
        void remove_empty_level(Price price, OrderSide side);
        void update_level_stats(PriceLevel *level, Quantity delta, bool add_order);
        Order *find_order(OrderId order_id);
        void remove_order_from_level(Order *order);

        // Non-locking versions for internal use
        std::pair<Price, Quantity> get_best_bid_internal() const;
        std::pair<Price, Quantity> get_best_ask_internal() const;
        Price get_mid_price_internal() const;
        Price get_spread_internal() const;
    };

    class OrderBookManager
    {
    public:
        OrderBookManager();
        ~OrderBookManager() = default;

        OrderBookManager(const OrderBookManager &) = delete;
        OrderBookManager &operator=(const OrderBookManager &) = delete;
        OrderBookManager(OrderBookManager &&) = delete;
        OrderBookManager &operator=(OrderBookManager &&) = delete;

        OrderBook *get_order_book(SymbolId symbol);
        bool add_order(SymbolId symbol, OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type = OrderType::LIMIT);
        bool cancel_order(SymbolId symbol, OrderId order_id, Quantity quantity = 0);
        bool modify_order(SymbolId symbol, OrderId order_id, Price new_price, Quantity new_quantity);
        bool execute_trade(SymbolId symbol, Price price, Quantity quantity, OrderSide side);
        const OrderBook *get_order_book(SymbolId symbol) const;
        std::vector<SymbolId> get_active_symbols() const;
        size_t order_book_count() const { return order_books_.size(); }

    private:
        std::map<SymbolId, std::unique_ptr<OrderBook>> order_books_;
        mutable std::mutex mutex_;
    };

} // namespace mm