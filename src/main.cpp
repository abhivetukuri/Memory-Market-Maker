#include "order_book.hpp"
#include "position_tracker.hpp"
#include "itch_parser.hpp"
#include "scenario_runner.hpp"
#include "types.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <filesystem>

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

    OrderBook order_book(1); // Symbol 1
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Price> price_dist(price_from_dollars(100.0), price_from_dollars(200.0));
    std::uniform_int_distribution<Quantity> qty_dist(100, 1000);
    std::uniform_int_distribution<OrderId> order_id_dist(1, 1000000);

    const int num_operations = 100000;

    // Benchmark add orders
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
    std::uniform_int_distribution<Price> price_dist(price_from_dollars(100.0), price_from_dollars(200.0));
    std::uniform_int_distribution<Quantity> qty_dist(100, 1000);
    std::uniform_int_distribution<OrderId> order_id_dist(1, 1000000);
    std::uniform_int_distribution<SymbolId> symbol_dist(1, 10);

    const int num_trades = 50000;

    // Benchmark trade recording
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_trades; ++i)
    {
        SymbolId symbol = symbol_dist(gen);
        Price price = price_dist(gen);
        Quantity qty = qty_dist(gen);
        OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;
        OrderId order_id = order_id_dist(gen);

        position_tracker.record_trade(symbol, price, qty, side, order_id);
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

    // Create order book and position tracker
    OrderBook order_book(1);
    PositionLimits limits;
    limits.max_position_size = 10000;
    limits.max_long_position = 5000;
    limits.max_short_position = 5000;
    PositionTracker position_tracker(limits);

    // Initial market making orders
    std::cout << "Placing initial market making orders..." << std::endl;

    // Place bid at $100.00
    order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY);
    std::cout << "Placed bid: 1000 @ $100.00" << std::endl;

    // Place ask at $100.10
    order_book.add_order(2, price_from_dollars(100.10), 1000, OrderSide::SELL);
    std::cout << "Placed ask: 1000 @ $100.10" << std::endl;

    print_order_book_stats(order_book);

    // Simulate a trade against our bid
    std::cout << "\nSimulating trade against our bid..." << std::endl;
    bool executed = order_book.execute_trade(price_from_dollars(100.00), 500, OrderSide::SELL);
    if (executed)
    {
        position_tracker.record_trade(1, price_from_dollars(100.00), 500, OrderSide::BUY, 1);
        std::cout << "Executed: 500 @ $100.00 (BUY)" << std::endl;
    }

    print_order_book_stats(order_book);
    print_position_stats(position_tracker);

    // Simulate a trade against our ask
    std::cout << "\nSimulating trade against our ask..." << std::endl;
    executed = order_book.execute_trade(price_from_dollars(100.10), 300, OrderSide::BUY);
    if (executed)
    {
        position_tracker.record_trade(1, price_from_dollars(100.10), 300, OrderSide::SELL, 2);
        std::cout << "Executed: 300 @ $100.10 (SELL)" << std::endl;
    }

    print_order_book_stats(order_book);
    print_position_stats(position_tracker);

    // Update unrealized P&L with current market price
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

        // Print order book and position statistics
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

    // Print detailed results for first few scenarios
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

int main()
{
    std::cout << "Memory Market Maker - C++20 Implementation" << std::endl;
    std::cout << "===========================================" << std::endl;

    try
    {
        // Test basic functionality
        test_market_making_scenario();

        // Run performance benchmarks
        benchmark_order_book_operations();
        benchmark_position_tracker();

        // Test ITCH data processing
        test_itch_data_processing();

        // Test scenario runner
        test_scenario_runner();

        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}