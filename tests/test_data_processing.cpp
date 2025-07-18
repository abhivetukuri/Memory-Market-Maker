#include "itch_parser.hpp"
#include "scenario_runner.hpp"
#include "order_book.hpp"
#include "position_tracker.hpp"
#include "types.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <random>

using namespace mm;

void test_itch_parser_small_sample()
{
    std::cout << "\n=== Testing ITCH Parser with Small Sample ===" << std::endl;

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
        std::cout << "ITCH file not found, skipping test." << std::endl;
        return;
    }

    std::ifstream file(itch_file, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Failed to open ITCH file for testing." << std::endl;
        return;
    }

    const size_t test_size = 1024 * 1024; // 1MB
    std::vector<uint8_t> buffer(test_size);
    file.read(reinterpret_cast<char *>(buffer.data()), test_size);
    size_t bytes_read = file.gcount();
    file.close();

    std::cout << "Testing with " << bytes_read << " bytes of ITCH data..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    size_t offset = 0;
    size_t messages_processed = 0;

    while (offset < bytes_read)
    {
        if (offset + 2 > bytes_read)
            break;

        uint16_t message_length = (buffer[offset] << 8) | buffer[offset + 1];
        if (offset + message_length > bytes_read)
            break;

        if (parser.process_message(&buffer[offset], message_length))
        {
            messages_processed++;
        }

        offset += message_length;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    auto stats = parser.get_stats();

    std::cout << "ITCH Parser Test Results:" << std::endl;
    std::cout << "  Messages Processed: " << messages_processed << std::endl;
    std::cout << "  Total Messages: " << stats.total_messages << std::endl;
    std::cout << "  Add Orders: " << stats.add_orders << std::endl;
    std::cout << "  Executions: " << stats.executions << std::endl;
    std::cout << "  Cancels: " << stats.cancels << std::endl;
    std::cout << "  Deletes: " << stats.deletes << std::endl;
    std::cout << "  Replaces: " << stats.replaces << std::endl;
    std::cout << "  Trades: " << stats.trades << std::endl;
    std::cout << "  Errors: " << stats.errors << std::endl;
    std::cout << "  Processing Time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Throughput: " << (messages_processed * 1000000.0 / duration.count()) << " messages/second" << std::endl;

    auto symbols = order_books.get_active_symbols();
    std::cout << "  Active Symbols: " << symbols.size() << std::endl;

    if (!symbols.empty())
    {
        std::cout << "  Sample Order Book Statistics:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(3), symbols.size()); ++i)
        {
            SymbolId symbol = symbols[i];
            const OrderBook *order_book = order_books.get_order_book(symbol);
            if (order_book)
            {
                auto book_stats = order_book->get_stats();
                std::cout << "    Symbol " << symbol << ":" << std::endl;
                std::cout << "      Orders: " << book_stats.total_orders << std::endl;
                std::cout << "      Bid Levels: " << book_stats.bid_levels << std::endl;
                std::cout << "      Ask Levels: " << book_stats.ask_levels << std::endl;
                std::cout << "      Best Bid: " << price_to_dollars(book_stats.best_bid) << std::endl;
                std::cout << "      Best Ask: " << price_to_dollars(book_stats.best_ask) << std::endl;
            }
        }
    }
}

void test_scenario_runner_individual()
{
    std::cout << "\n=== Testing Individual Scenarios ===" << std::endl;

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
        std::cout << "Scenarios directory not found, skipping test." << std::endl;
        return;
    }

    std::vector<std::string> test_scenarios = {"scenario-01", "scenario-03", "scenario-06", "scenario-07"};

    for (const auto &scenario_name : test_scenarios)
    {
        std::string filename = scenarios_dir + "/" + scenario_name + ".txt";
        if (!std::filesystem::exists(filename))
        {
            std::cout << "Scenario file not found: " << filename << std::endl;
            continue;
        }

        std::cout << "\nTesting scenario: " << scenario_name << std::endl;

        OrderBookManager new_order_books;
        PositionTracker new_position_tracker(limits);
        ScenarioRunner new_runner(new_order_books, new_position_tracker);

        auto result = new_runner.run_scenario(filename);

        std::cout << "  Status: " << (result.passed ? "PASSED" : "FAILED") << std::endl;
        std::cout << "  Execution Time: " << result.execution_time_ms << " ms" << std::endl;
        std::cout << "  Orders Processed: " << result.orders_processed << std::endl;
        std::cout << "  Trades Executed: " << result.trades_executed << std::endl;

        if (!result.passed)
        {
            std::cout << "  Error: " << result.error_message << std::endl;
        }

        if (scenario_name == "scenario-06" || scenario_name == "scenario-07")
        {
            std::cout << "  Final Order Book State:" << std::endl;
            for (const auto &[symbol, stats] : result.order_book_stats)
            {
                std::cout << "    Symbol " << symbol << ":" << std::endl;
                std::cout << "      Orders: " << stats.total_orders << std::endl;
                std::cout << "      Bid Levels: " << stats.bid_levels << std::endl;
                std::cout << "      Ask Levels: " << stats.ask_levels << std::endl;
                std::cout << "      Best Bid: " << price_to_dollars(stats.best_bid) << std::endl;
                std::cout << "      Best Ask: " << price_to_dollars(stats.best_ask) << std::endl;
                std::cout << "      Spread: " << price_to_dollars(stats.spread) << std::endl;
            }

            std::cout << "  Final Position State:" << std::endl;
            std::cout << "    Total Symbols: " << result.position_stats.total_symbols << std::endl;
            std::cout << "    Active Positions: " << result.position_stats.active_positions << std::endl;
            std::cout << "    Total P&L: " << price_to_dollars(result.position_stats.total_pnl) << std::endl;
        }
    }
}

void test_scenario_runner_performance()
{
    std::cout << "\n=== Scenario Runner Performance Test ===" << std::endl;

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
        std::cout << "Scenarios directory not found, skipping test." << std::endl;
        return;
    }

    const int num_iterations = 10;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i)
    {
        OrderBookManager new_order_books;
        PositionTracker new_position_tracker(limits);
        ScenarioRunner new_runner(new_order_books, new_position_tracker);

        auto results = new_runner.run_all_scenarios(scenarios_dir);

        if (i == 0)
        {
            size_t passed = 0;
            size_t failed = 0;
            for (const auto &result : results)
            {
                if (result.passed)
                    passed++;
                else
                    failed++;
            }
            std::cout << "First iteration results:" << std::endl;
            std::cout << "  Total Scenarios: " << results.size() << std::endl;
            std::cout << "  Passed: " << passed << std::endl;
            std::cout << "  Failed: " << failed << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Performance Test Results:" << std::endl;
    std::cout << "  Iterations: " << num_iterations << std::endl;
    std::cout << "  Total Time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Average Time per Iteration: " << (duration.count() / num_iterations) << " ms" << std::endl;

    size_t total_scenarios = 0;
    for (const auto &entry : std::filesystem::directory_iterator(scenarios_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".txt")
        {
            total_scenarios++;
        }
    }

    std::cout << "  Total Scenarios per Iteration: " << total_scenarios << std::endl;
    std::cout << "  Average Time per Scenario: " << (duration.count() / (num_iterations * total_scenarios)) << " ms" << std::endl;
    std::cout << "  Scenarios per Second: " << (total_scenarios * num_iterations * 1000.0 / duration.count()) << std::endl;
}

void test_memory_efficiency()
{
    std::cout << "\n=== Memory Efficiency Test ===" << std::endl;

    OrderBook order_book(1);
    PositionLimits limits;
    limits.max_position_size = 1000000;
    limits.max_long_position = 500000;
    limits.max_short_position = 500000;
    PositionTracker position_tracker(limits);

    const int num_orders = 100000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Price> price_dist(price_from_dollars(100.0), price_from_dollars(200.0));
    std::uniform_int_distribution<Quantity> qty_dist(100, 1000);

    std::cout << "Adding " << num_orders << " orders..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_orders; ++i)
    {
        OrderId order_id = i + 1;
        Price price = price_dist(gen);
        Quantity qty = qty_dist(gen);
        OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;

        order_book.add_order(order_id, price, qty, side);

        if (i % 1000 == 0)
        {
            position_tracker.record_trade(1, price, qty, side, order_id);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    auto order_book_stats = order_book.get_stats();
    auto position_stats = position_tracker.get_stats();

    std::cout << "Memory Efficiency Test Results:" << std::endl;
    std::cout << "  Orders Added: " << num_orders << std::endl;
    std::cout << "  Processing Time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Orders per Second: " << (num_orders * 1000.0 / duration.count()) << std::endl;
    std::cout << "  Order Book Stats:" << std::endl;
    std::cout << "    Total Orders: " << order_book_stats.total_orders << std::endl;
    std::cout << "    Active Orders: " << order_book_stats.active_orders << std::endl;
    std::cout << "    Bid Levels: " << order_book_stats.bid_levels << std::endl;
    std::cout << "    Ask Levels: " << order_book_stats.ask_levels << std::endl;
    std::cout << "  Position Tracker Stats:" << std::endl;
    std::cout << "    Total Symbols: " << position_stats.total_symbols << std::endl;
    std::cout << "    Active Positions: " << position_stats.active_positions << std::endl;
    std::cout << "    Total P&L: " << price_to_dollars(position_stats.total_pnl) << std::endl;
}

int main()
{
    std::cout << "Memory Market Maker - Data Processing Tests" << std::endl;
    std::cout << "============================================" << std::endl;

    try
    {
        test_itch_parser_small_sample();

        test_scenario_runner_individual();

        test_scenario_runner_performance();

        test_memory_efficiency();

        std::cout << "\n=== All data processing tests completed! ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}