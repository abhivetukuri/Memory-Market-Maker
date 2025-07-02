#include "strategy.hpp"
#include <algorithm>
#include <iostream>

namespace mm
{

    FixedSpreadStrategy::FixedSpreadStrategy(const Config &cfg)
        : config_(cfg)
    {
        for (size_t i = 0; i < MAX_STRATEGY_SYMBOLS; ++i)
        {
            state_[i] = SymbolState{0, 0, 0, 0, 0};
        }
    }

    void FixedSpreadStrategy::update_quotes(OrderBookManager &order_books, PositionTracker &, Timestamp)
    {
        for (size_t i = 0; i < config_.num_symbols; ++i)
        {
            SymbolId symbol = config_.symbols[i];
            auto &s = state_[i];
            Price mid = config_.base_price;
            Price bid = mid - config_.spread / 2;
            Price ask = mid + config_.spread / 2;
            Quantity qty = config_.quote_size;

            // Cancel previous orders if price/qty changed
            if (s.bid_order_id)
            {
                order_books.cancel_order(symbol, s.bid_order_id);
            }
            if (s.ask_order_id)
            {
                order_books.cancel_order(symbol, s.ask_order_id);
            }

            // Place new bid/ask
            s.bid_order_id = 10000 + i * 2 + 1; // deterministic order ids
            s.ask_order_id = 10000 + i * 2 + 2;
            order_books.add_order(symbol, s.bid_order_id, bid, qty, OrderSide::BUY);
            order_books.add_order(symbol, s.ask_order_id, ask, qty, OrderSide::SELL);
            s.last_bid = bid;
            s.last_ask = ask;
            s.last_qty = qty;
        }
    }

    void FixedSpreadStrategy::on_trade(SymbolId, Price, Quantity, OrderSide, Timestamp)
    {
        // No-op for fixed spread
    }

    void FixedSpreadStrategy::on_position_update(SymbolId, const Position &, const PositionTracker::Stats &, Timestamp)
    {
        // No-op for fixed spread
    }

    InventorySkewedStrategy::InventorySkewedStrategy(const Config &cfg)
        : config_(cfg)
    {
        for (size_t i = 0; i < MAX_STRATEGY_SYMBOLS; ++i)
        {
            state_[i] = SymbolState{0, 0, 0, 0, 0, 0};
        }
    }

    void InventorySkewedStrategy::update_quotes(OrderBookManager &order_books, PositionTracker &positions, Timestamp)
    {
        for (size_t i = 0; i < config_.num_symbols; ++i)
        {
            SymbolId symbol = config_.symbols[i];
            auto &s = state_[i];
            // Get inventory for this symbol
            const Position *pos = positions.get_position(symbol);
            Quantity inv = pos ? pos->get_net_position() : 0;
            s.inventory = inv;
            // Skew mid price based on inventory
            double skew = double(inv) / double(config_.max_inventory);
            Price mid = config_.base_price - Price(skew * double(config_.max_spread) / 2);
            // Spread widens as inventory increases
            Price spread = config_.min_spread + Price(std::abs(skew) * double(config_.max_spread - config_.min_spread));
            Price bid = mid - spread / 2;
            Price ask = mid + spread / 2;
            Quantity qty = config_.quote_size;

            // Cancel previous orders if price/qty changed
            if (s.bid_order_id)
            {
                order_books.cancel_order(symbol, s.bid_order_id);
            }
            if (s.ask_order_id)
            {
                order_books.cancel_order(symbol, s.ask_order_id);
            }

            // Place new bid/ask
            s.bid_order_id = 20000 + i * 2 + 1;
            s.ask_order_id = 20000 + i * 2 + 2;
            order_books.add_order(symbol, s.bid_order_id, bid, qty, OrderSide::BUY);
            order_books.add_order(symbol, s.ask_order_id, ask, qty, OrderSide::SELL);
            s.last_bid = bid;
            s.last_ask = ask;
            s.last_qty = qty;
        }
    }

    void InventorySkewedStrategy::on_trade(SymbolId, Price, Quantity, OrderSide, Timestamp)
    {
        // No-op for now; could be used for advanced logic
    }

    void InventorySkewedStrategy::on_position_update(SymbolId, const Position &, const PositionTracker::Stats &, Timestamp)
    {
        // No-op for now; could be used for advanced logic
    }

} // namespace mm