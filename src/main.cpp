#include "order_book.hpp"
#include "position_tracker.hpp"
#include "itch_parser.hpp"
#include "scenario_runner.hpp"
#include "strategy.hpp"
#include "types.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <filesystem>
#include <thread>

using namespace mm;

void print_order_book_stats(const OrderBook &order_book)
{
    auto stats = order_book.get_stats();
    std::cout << "Order Book Stats:" << std::endl;
    std::cout << "  Total Orders: " << stats.total_orders << std::endl;
    std::cout << "  Active Orders: " << stats.active_orders << std::endl;
    std::cout << "  Bid Levels: " << stats.bid_levels << std::endl;
    std::cout << "  Ask Levels: " << stats.ask_levels << std::endl;
    std::cout << "  Best Bid: " << price_to_dollars(stats.best_bid) << " (" << stats.best_bid << ")" << std::endl;
    std::cout << "  Best Ask: " << price_to_dollars(stats.best_ask) << " (" << stats.best_ask << ")" << std::endl;
    std::cout << "  Mid Price: " << price_to_dollars(stats.mid_price) << " (" << stats.mid_price << ")" << std::endl;
    std::cout << "  Spread: " << price_to_dollars(stats.spread) << " (" << stats.spread << ")" << std::endl;
}

void print_position_stats(const PositionTracker &position_tracker)
{
    auto stats = position_tracker.get_stats();
    std::cout << "Position Stats:" << std::endl;
    std::cout << "  Total Symbols: " << stats.total_symbols << std::endl;
    std::cout << "  Active Positions: " << stats.active_positions << std::endl;
    std::cout << "  Total Realized P&L: " << price_to_dollars(stats.total_realized_pnl) << " (" << stats.total_realized_pnl << ")" << std::endl;
    std::cout << "  Total Unrealized P&L: " << price_to_dollars(stats.total_unrealized_pnl) << " (" << stats.total_unrealized_pnl << ")" << std::endl;
    std::cout << "  Total P&L: " << price_to_dollars(stats.total_pnl) << " (" << stats.total_pnl << ")" << std::endl;
    std::cout << "  Max Position Size: " << stats.max_position_size << std::endl;
    std::cout << "  Largest Position Symbol: " << stats.largest_position_symbol << std::endl;
}

void benchmark_order_book_operations()
{
    std::cout << "\n=== Order Book Performance Benchmark ===" << std::endl;

    OrderBook order_book(1);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Price> price_dist(price_from_dollars(100.0), price_from_dollars(101.0));
    std::uniform_int_distribution<Quantity> qty_dist(100, 1000);
    std::uniform_int_distribution<OrderId> order_id_dist(1, 1000000);

    const int num_operations = 100000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i)
    {
        OrderId order_id = order_id_dist(gen);
        Price price = price_dist(gen);
        Quantity qty = qty_dist(gen);
        OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;

        order_book.add_order(order_id, price, qty, side);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Added " << num_operations << " orders in "
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average time per order: "
              << static_cast<double>(duration.count()) / num_operations << " microseconds" << std::endl;
    std::cout << "Throughput: "
              << static_cast<double>(num_operations) * 1000000 / duration.count() << " orders/second" << std::endl;

    print_order_book_stats(order_book);
}

void benchmark_position_tracker()
{
    std::cout << "\n=== Position Tracker Performance Benchmark ===" << std::endl;

    PositionLimits limits;
    limits.max_position_size = 100000;
    limits.max_long_position = 50000;
    limits.max_short_position = 50000;

    PositionTracker position_tracker(limits);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Quantity> qty_dist(100, 1000);
    std::uniform_int_distribution<OrderId> order_id_dist(1, 1000000);
    std::uniform_int_distribution<SymbolId> symbol_dist(1, 10);

    const int num_trades = 50000;
    const Price base_price = price_from_dollars(100.0);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_trades; ++i)
    {
        SymbolId symbol = symbol_dist(gen);
        Quantity qty = qty_dist(gen);
        OrderId order_id = order_id_dist(gen);

        // Simulate realistic market making: alternating buy/sell with small spread
        if (i % 2 == 0)
        {
            // Buy at slightly lower price
            Price buy_price = base_price - price_from_dollars(0.01);
            position_tracker.record_trade(symbol, buy_price, qty, OrderSide::BUY, order_id);
        }
        else
        {
            // Sell at slightly higher price (profitable)
            Price sell_price = base_price + price_from_dollars(0.02);
            position_tracker.record_trade(symbol, sell_price, qty, OrderSide::SELL, order_id);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Recorded " << num_trades << " trades in "
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average time per trade: "
              << static_cast<double>(duration.count()) / num_trades << " microseconds" << std::endl;
    std::cout << "Throughput: "
              << static_cast<double>(num_trades) * 1000000 / duration.count() << " trades/second" << std::endl;

    print_position_stats(position_tracker);
}

void test_market_making_scenario()
{
    std::cout << "\n=== Market Making Scenario Test ===" << std::endl;

    OrderBook order_book(1);
    PositionLimits limits;
    limits.max_position_size = 10000;
    limits.max_long_position = 5000;
    limits.max_short_position = 5000;
    PositionTracker position_tracker(limits);

    std::cout << "Placing initial market making orders..." << std::endl;

    order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY);
    std::cout << "Placed bid: 1000 @ $100.00" << std::endl;

    order_book.add_order(2, price_from_dollars(100.10), 1000, OrderSide::SELL);
    std::cout << "Placed ask: 1000 @ $100.10" << std::endl;

    print_order_book_stats(order_book);

    std::cout << "\nSimulating trade against our bid..." << std::endl;
    bool executed = order_book.execute_trade(price_from_dollars(100.00), 500, OrderSide::SELL);
    if (executed)
    {
        position_tracker.record_trade(1, price_from_dollars(100.00), 500, OrderSide::BUY, 1);
        std::cout << "Executed: 500 @ $100.00 (BUY)" << std::endl;
    }

    print_order_book_stats(order_book);
    print_position_stats(position_tracker);

    std::cout << "\nSimulating trade against our ask..." << std::endl;
    executed = order_book.execute_trade(price_from_dollars(100.10), 300, OrderSide::BUY);
    if (executed)
    {
        position_tracker.record_trade(1, price_from_dollars(100.10), 300, OrderSide::SELL, 2);
        std::cout << "Executed: 300 @ $100.10 (SELL)" << std::endl;
    }

    print_order_book_stats(order_book);
    print_position_stats(position_tracker);

    std::cout << "\nUpdating unrealized P&L..." << std::endl;
    Price current_price = price_from_dollars(100.05); // Mid price
    position_tracker.update_unrealized_pnl(1, current_price);

    print_position_stats(position_tracker);
}

void test_itch_data_processing()
{
    std::cout << "\n=== ITCH Data Processing Test ===" << std::endl;

    OrderBookManager order_books;
    PositionLimits limits;
    limits.max_position_size = 100000;
    limits.max_long_position = 50000;
    limits.max_short_position = 50000;
    PositionTracker position_tracker(limits);

    ITCHParser parser(order_books, position_tracker);

    std::string itch_file = "data/sample.itch";
    if (!std::filesystem::exists(itch_file))
    {
        std::cout << "ITCH file not found: " << itch_file << std::endl;
        std::cout << "Skipping ITCH data processing test." << std::endl;
        return;
    }

    std::cout << "Processing ITCH file: " << itch_file << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    bool success = parser.parse_file(itch_file);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (success)
    {
        auto stats = parser.get_stats();
        std::cout << "ITCH Processing Results:" << std::endl;
        std::cout << "  Total Messages: " << stats.total_messages << std::endl;
        std::cout << "  Add Orders: " << stats.add_orders << std::endl;
        std::cout << "  Executions: " << stats.executions << std::endl;
        std::cout << "  Cancels: " << stats.cancels << std::endl;
        std::cout << "  Deletes: " << stats.deletes << std::endl;
        std::cout << "  Replaces: " << stats.replaces << std::endl;
        std::cout << "  Trades: " << stats.trades << std::endl;
        std::cout << "  Errors: " << stats.errors << std::endl;
        std::cout << "  Processing Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Throughput: " << (stats.total_messages * 1000.0 / duration.count()) << " messages/second" << std::endl;

        auto symbols = order_books.get_active_symbols();
        std::cout << "  Active Symbols: " << symbols.size() << std::endl;

        if (!symbols.empty())
        {
            SymbolId first_symbol = symbols[0];
            const OrderBook *order_book = order_books.get_order_book(first_symbol);
            if (order_book)
            {
                print_order_book_stats(*order_book);
            }
        }

        print_position_stats(position_tracker);
    }
    else
    {
        std::cout << "Failed to process ITCH file." << std::endl;
    }
}

void test_scenario_runner()
{
    std::cout << "\n=== Scenario Runner Test ===" << std::endl;

    OrderBookManager order_books;
    PositionLimits limits;
    limits.max_position_size = 100000;
    limits.max_long_position = 50000;
    limits.max_short_position = 50000;
    PositionTracker position_tracker(limits);

    ScenarioRunner runner(order_books, position_tracker);

    std::string scenarios_dir = "data/matching";
    if (!std::filesystem::exists(scenarios_dir))
    {
        std::cout << "Scenarios directory not found: " << scenarios_dir << std::endl;
        std::cout << "Skipping scenario runner test." << std::endl;
        return;
    }

    std::cout << "Running scenarios from: " << scenarios_dir << std::endl;

    auto results = runner.run_all_scenarios(scenarios_dir);

    std::cout << "Scenario Results:" << std::endl;
    std::cout << "  Total Scenarios: " << results.size() << std::endl;

    size_t passed = 0;
    size_t failed = 0;
    double total_time = 0;

    for (const auto &result : results)
    {
        if (result.passed)
        {
            passed++;
        }
        else
        {
            failed++;
            std::cout << "  FAILED: " << result.scenario_name << " - " << result.error_message << std::endl;
        }
        total_time += result.execution_time_ms;
    }

    std::cout << "  Passed: " << passed << std::endl;
    std::cout << "  Failed: " << failed << std::endl;
    std::cout << "  Total Execution Time: " << total_time << " ms" << std::endl;
    std::cout << "  Average Execution Time: " << (total_time / results.size()) << " ms" << std::endl;

    std::cout << "\nDetailed Results (first 3 scenarios):" << std::endl;
    for (size_t i = 0; i < std::min(size_t(3), results.size()); ++i)
    {
        const auto &result = results[i];
        std::cout << "  " << result.scenario_name << ":" << std::endl;
        std::cout << "    Status: " << (result.passed ? "PASSED" : "FAILED") << std::endl;
        std::cout << "    Execution Time: " << result.execution_time_ms << " ms" << std::endl;
        std::cout << "    Orders Processed: " << result.orders_processed << std::endl;
        std::cout << "    Trades Executed: " << result.trades_executed << std::endl;
    }
}

void test_strategy_simulation()
{
    std::cout << "\n=== Market Making Strategy Simulation ===" << std::endl;

    constexpr size_t num_symbols = 2;
    std::array<SymbolId, MAX_STRATEGY_SYMBOLS> symbols = {1, 2};

    std::unique_ptr<OrderBookManager> order_books = std::make_unique<OrderBookManager>();
    PositionLimits limits;
    limits.max_position_size = 10000;
    limits.max_long_position = 5000;
    limits.max_short_position = 5000;
    std::unique_ptr<PositionTracker> position_tracker = std::make_unique<PositionTracker>(limits);

    FixedSpreadStrategy::Config fixed_cfg;
    fixed_cfg.base_price = price_from_dollars(100.00);
    fixed_cfg.spread = price_from_dollars(0.10);
    fixed_cfg.quote_size = 100;
    fixed_cfg.num_symbols = num_symbols;
    fixed_cfg.symbols = symbols;
    FixedSpreadStrategy fixed_strategy(fixed_cfg);

    InventorySkewedStrategy::Config inv_cfg;
    inv_cfg.base_price = price_from_dollars(100.00);
    inv_cfg.min_spread = price_from_dollars(0.05);
    inv_cfg.max_spread = price_from_dollars(0.20);
    inv_cfg.quote_size = 100;
    inv_cfg.max_inventory = 1000;
    inv_cfg.num_symbols = num_symbols;
    inv_cfg.symbols = symbols;
    InventorySkewedStrategy inv_strategy(inv_cfg);

    for (int strat = 0; strat < 2; ++strat)
    {
        std::cout << "\n--- Simulating " << (strat == 0 ? "FixedSpreadStrategy" : "InventorySkewedStrategy") << " ---" << std::endl;
        order_books = std::make_unique<OrderBookManager>();
        position_tracker = std::make_unique<PositionTracker>(limits);
        MarketMakingStrategy *strategy = (strat == 0) ? (MarketMakingStrategy *)&fixed_strategy : (MarketMakingStrategy *)&inv_strategy;

        std::mt19937 gen(42 + strat);
        std::uniform_int_distribution<int> symbol_dist(0, num_symbols - 1);
        std::uniform_real_distribution<double> trade_prob(0.0, 1.0);

        for (int round = 0; round < 20; ++round)
        {
            Timestamp now = round * 1000000;
            strategy->update_quotes(*order_books, *position_tracker, now);
            for (size_t i = 0; i < num_symbols; ++i)
            {
                SymbolId symbol = symbols[i];
                const OrderBook *ob = order_books->get_order_book(symbol);
                if (!ob)
                    continue;
                auto [bid, bid_qty] = ob->get_best_bid();
                auto [ask, ask_qty] = ob->get_best_ask();
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
                const Position *pos = position_tracker->get_position(symbol);
                if (pos)
                {
                    strategy->on_position_update(symbol, *pos, position_tracker->get_stats(), now);
                }
            }
        }
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
    std::cout << "Memory Market Maker - C++20 Implementation" << std::endl;
    std::cout << "===========================================" << std::endl;

    try
    {
        test_market_making_scenario();

        benchmark_order_book_operations();
        benchmark_position_tracker();

        test_itch_data_processing();

        test_scenario_runner();

        test_strategy_simulation();

        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}