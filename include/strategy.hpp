#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include "position_tracker.hpp"
#include <array>
#include <cstddef>

namespace mm
{

    // Maximum number of symbols supported by strategies
    constexpr size_t MAX_STRATEGY_SYMBOLS = 16;

    // MarketMakingStrategy interface
    class MarketMakingStrategy
    {
    public:
        virtual ~MarketMakingStrategy() = default;

        // Called to update quotes for all symbols
        virtual void update_quotes(OrderBookManager &order_books, PositionTracker &positions, Timestamp now) = 0;

        // Called to notify strategy of a trade
        virtual void on_trade(SymbolId symbol, Price price, Quantity qty, OrderSide side, Timestamp now) = 0;

        // Called to notify strategy of a position or P&L update
        virtual void on_position_update(SymbolId symbol, const Position &pos, const PositionTracker::Stats &stats, Timestamp now) = 0;
    };

    // Fixed spread market making strategy
    class FixedSpreadStrategy : public MarketMakingStrategy
    {
    public:
        struct Config
        {
            Price base_price;
            Price spread;
            Quantity quote_size;
            size_t num_symbols;
            std::array<SymbolId, MAX_STRATEGY_SYMBOLS> symbols;
        };

        explicit FixedSpreadStrategy(const Config &cfg);
        void update_quotes(OrderBookManager &order_books, PositionTracker &positions, Timestamp now) override;
        void on_trade(SymbolId symbol, Price price, Quantity qty, OrderSide side, Timestamp now) override;
        void on_position_update(SymbolId symbol, const Position &pos, const PositionTracker::Stats &stats, Timestamp now) override;

    private:
        Config config_;
        // Pre-allocated state for each symbol
        struct SymbolState
        {
            OrderId bid_order_id;
            OrderId ask_order_id;
            Price last_bid;
            Price last_ask;
            Quantity last_qty;
        };
        std::array<SymbolState, MAX_STRATEGY_SYMBOLS> state_;
    };

    // Inventory-skewed market making strategy
    class InventorySkewedStrategy : public MarketMakingStrategy
    {
    public:
        struct Config
        {
            Price base_price;
            Price min_spread;
            Price max_spread;
            Quantity quote_size;
            Quantity max_inventory;
            size_t num_symbols;
            std::array<SymbolId, MAX_STRATEGY_SYMBOLS> symbols;
        };

        explicit InventorySkewedStrategy(const Config &cfg);
        void update_quotes(OrderBookManager &order_books, PositionTracker &positions, Timestamp now) override;
        void on_trade(SymbolId symbol, Price price, Quantity qty, OrderSide side, Timestamp now) override;
        void on_position_update(SymbolId symbol, const Position &pos, const PositionTracker::Stats &stats, Timestamp now) override;

    private:
        Config config_;
        struct SymbolState
        {
            OrderId bid_order_id;
            OrderId ask_order_id;
            Price last_bid;
            Price last_ask;
            Quantity last_qty;
            Quantity inventory;
        };
        std::array<SymbolState, MAX_STRATEGY_SYMBOLS> state_;
    };

} // namespace mm