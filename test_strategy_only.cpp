#include "strategy.hpp"
#include "order_book.hpp"
#include "position_tracker.hpp"
#include "types.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <memory>

using namespace mm;

void test_strategy_simulation()
{
    std::cout << "=== Market Making Strategy Simulation ===" << std::endl;

    constexpr size_t num_symbols = 2;
    std::array<SymbolId, MAX_STRATEGY_SYMBOLS> symbols = {1, 2};

    // Use unique_ptr for resettable state
    std::unique_ptr<OrderBookManager> order_books = std::make_unique<OrderBookManager>();
    PositionLimits limits;
    limits.max_position_size = 10000;
    limits.max_long_position = 5000;
    limits.max_short_position = 5000;
    std::unique_ptr<PositionTracker> position_tracker = std::make_unique<PositionTracker>(limits);

    // Fixed spread strategy config
    FixedSpreadStrategy::Config fixed_cfg;
    fixed_cfg.base_price = price_from_dollars(100.00);
    fixed_cfg.spread = price_from_dollars(0.10);
    fixed_cfg.quote_size = 100;
    fixed_cfg.num_symbols = num_symbols;
    fixed_cfg.symbols = symbols;
    FixedSpreadStrategy fixed_strategy(fixed_cfg);

    // Inventory-skewed strategy config
    InventorySkewedStrategy::Config inv_cfg;
    inv_cfg.base_price = price_from_dollars(100.00);
    inv_cfg.min_spread = price_from_dollars(0.05);
    inv_cfg.max_spread = price_from_dollars(0.20);
    inv_cfg.quote_size = 100;
    inv_cfg.max_inventory = 1000;
    inv_cfg.num_symbols = num_symbols;
    inv_cfg.symbols = symbols;
    InventorySkewedStrategy inv_strategy(inv_cfg);

    // Simulate a simple market for both strategies
    for (int strat = 0; strat < 2; ++strat)
    {
        std::cout << "\n--- Simulating " << (strat == 0 ? "FixedSpreadStrategy" : "InventorySkewedStrategy") << " ---" << std::endl;
        // Reset state
        order_books = std::make_unique<OrderBookManager>();
        position_tracker = std::make_unique<PositionTracker>(limits);
        MarketMakingStrategy *strategy = (strat == 0) ? (MarketMakingStrategy *)&fixed_strategy : (MarketMakingStrategy *)&inv_strategy;

        // Simulate 20 rounds of quoting and random trades
        std::mt19937 gen(42 + strat);
        std::uniform_int_distribution<int> symbol_dist(0, num_symbols - 1);
        std::uniform_real_distribution<double> trade_prob(0.0, 1.0);

        for (int round = 0; round < 20; ++round)
        {
            Timestamp now = round * 1000000;
            strategy->update_quotes(*order_books, *position_tracker, now);
            // Simulate random trades against our quotes
            for (size_t i = 0; i < num_symbols; ++i)
            {
                SymbolId symbol = symbols[i];
                const OrderBook *ob = order_books->get_order_book(symbol);
                if (!ob)
                    continue;
                auto [bid, bid_qty] = ob->get_best_bid();
                auto [ask, ask_qty] = ob->get_best_ask();
                // 50% chance of trade on each side
                if (trade_prob(gen) < 0.5 && bid > 0)
                {
                    Quantity qty = 10 + (gen() % 20);
                    order_books->execute_trade(symbol, bid, qty, OrderSide::SELL);
                    position_tracker->record_trade(symbol, bid, qty, OrderSide::BUY, 100000 + round * 10 + i);
                    strategy->on_trade(symbol, bid, qty, OrderSide::BUY, now);
                }
                if (trade_prob(gen) < 0.5 && ask > 0)
                {
                    Quantity qty = 10 + (gen() % 20);
                    order_books->execute_trade(symbol, ask, qty, OrderSide::BUY);
                    position_tracker->record_trade(symbol, ask, qty, OrderSide::SELL, 200000 + round * 10 + i);
                    strategy->on_trade(symbol, ask, qty, OrderSide::SELL, now);
                }
                // Notify strategy of position update
                const Position *pos = position_tracker->get_position(symbol);
                if (pos)
                {
                    strategy->on_position_update(symbol, *pos, position_tracker->get_stats(), now);
                }
            }
        }
        // Print final stats
        for (size_t i = 0; i < num_symbols; ++i)
        {
            SymbolId symbol = symbols[i];
            const Position *pos = position_tracker->get_position(symbol);
            std::cout << "Symbol " << symbol << ": ";
            if (pos)
            {
                std::cout << "NetPos=" << pos->get_net_position() << ", RealizedPnL=" << price_to_dollars(pos->realized_pnl)
                          << ", UnrealizedPnL=" << price_to_dollars(pos->unrealized_pnl) << std::endl;
            }
            else
            {
                std::cout << "No position" << std::endl;
            }
        }
        auto stats = position_tracker->get_stats();
        std::cout << "Total P&L: " << price_to_dollars(stats.total_pnl) << std::endl;
    }
}

int main()
{
    test_strategy_simulation();
    return 0;
}