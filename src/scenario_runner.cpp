#include "scenario_runner.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <algorithm>

namespace mm
{

    ScenarioRunner::ScenarioRunner(OrderBookManager &order_books, PositionTracker &position_tracker)
        : order_books_(order_books), position_tracker_(position_tracker), matching_enabled_(false)
    {
    }

    ScenarioResult ScenarioRunner::run_scenario(const std::string &filename)
    {
        ScenarioResult result;
        result.scenario_name = std::filesystem::path(filename).stem().string();
        result.passed = true;
        result.error_message = "";
        result.orders_processed = 0;
        result.trades_executed = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        try
        {
            auto commands = parse_scenario_file(filename);

            for (const auto &command : commands)
            {
                if (command.type == ScenarioCommandType::COMMENT)
                {
                    continue;
                }

                if (!execute_command(command))
                {
                    result.passed = false;
                    result.error_message = "Failed to execute command at line " + std::to_string(command.line_number);
                    break;
                }

                if (command.type == ScenarioCommandType::ADD_LIMIT_BUY ||
                    command.type == ScenarioCommandType::ADD_LIMIT_SELL ||
                    command.type == ScenarioCommandType::ADD_MARKET_BUY ||
                    command.type == ScenarioCommandType::ADD_MARKET_SELL)
                {
                    result.orders_processed++;
                }
            }

            auto symbols = order_books_.get_active_symbols();
            for (SymbolId symbol : symbols)
            {
                const OrderBook *order_book = order_books_.get_order_book(symbol);
                if (order_book)
                {
                    result.order_book_stats[symbol] = order_book->get_stats();
                }
            }
            result.position_stats = position_tracker_.get_stats();
        }
        catch (const std::exception &e)
        {
            result.passed = false;
            result.error_message = "Exception: " + std::string(e.what());
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.execution_time_ms = duration.count();

        stats_.total_scenarios++;
        if (result.passed)
        {
            stats_.passed_scenarios++;
        }
        else
        {
            stats_.failed_scenarios++;
        }
        stats_.total_execution_time_ms += result.execution_time_ms;
        stats_.avg_execution_time_ms = stats_.total_execution_time_ms / stats_.total_scenarios;

        return result;
    }

    std::vector<ScenarioResult> ScenarioRunner::run_all_scenarios(const std::string &directory)
    {
        std::vector<ScenarioResult> results;

        try
        {
            for (const auto &entry : std::filesystem::directory_iterator(directory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".txt")
                {
                    auto result = run_scenario(entry.path().string());
                    results.push_back(result);
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error reading directory: " << e.what() << std::endl;
        }

        return results;
    }

    ScenarioResult ScenarioRunner::run_scenario_by_name(const std::string &scenario_name)
    {
        std::string filename = "data/matching/" + scenario_name + ".txt";
        return run_scenario(filename);
    }

    std::vector<ScenarioCommand> ScenarioRunner::parse_scenario_file(const std::string &filename)
    {
        std::vector<ScenarioCommand> commands;
        std::ifstream file(filename);

        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open scenario file: " + filename);
        }

        std::string line;
        size_t line_number = 0;

        while (std::getline(file, line))
        {
            line_number++;

            // Skip empty lines
            if (line.empty())
                continue;

            // Parse command
            auto command = parse_command_line(line, line_number);
            if (command.type != ScenarioCommandType::UNKNOWN)
            {
                commands.push_back(command);
            }
        }

        return commands;
    }

    bool ScenarioRunner::execute_command(const ScenarioCommand &command)
    {
        switch (command.type)
        {
        case ScenarioCommandType::ENABLE_MATCHING:
            return execute_enable_matching(command.arguments);
        case ScenarioCommandType::ADD_SYMBOL:
            return execute_add_symbol(command.arguments);
        case ScenarioCommandType::DELETE_SYMBOL:
            return execute_delete_symbol(command.arguments);
        case ScenarioCommandType::ADD_BOOK:
            return execute_add_book(command.arguments);
        case ScenarioCommandType::DELETE_BOOK:
            return execute_delete_book(command.arguments);
        case ScenarioCommandType::ADD_LIMIT_BUY:
            return execute_add_limit_buy(command.arguments);
        case ScenarioCommandType::ADD_LIMIT_SELL:
            return execute_add_limit_sell(command.arguments);
        case ScenarioCommandType::ADD_MARKET_BUY:
            return execute_add_market_buy(command.arguments);
        case ScenarioCommandType::ADD_MARKET_SELL:
            return execute_add_market_sell(command.arguments);
        case ScenarioCommandType::REDUCE_ORDER:
            return execute_reduce_order(command.arguments);
        case ScenarioCommandType::MODIFY_ORDER:
            return execute_modify_order(command.arguments);
        case ScenarioCommandType::REPLACE_ORDER:
            return execute_replace_order(command.arguments);
        case ScenarioCommandType::DELETE_ORDER:
            return execute_delete_order(command.arguments);
        case ScenarioCommandType::ADD_SLIPPAGE_MARKET_BUY:
            return execute_add_slippage_market_buy(command.arguments);
        case ScenarioCommandType::ADD_SLIPPAGE_MARKET_SELL:
            return execute_add_slippage_market_sell(command.arguments);
        case ScenarioCommandType::COMMENT:
            return true; // Comments are always successful
        default:
            return false;
        }
    }

    ScenarioCommand ScenarioRunner::parse_command_line(const std::string &line, size_t line_number)
    {
        ScenarioCommand command;
        command.line_number = line_number;
        command.comment = "";

        // Check if line is a comment
        if (line[0] == '#')
        {
            command.type = ScenarioCommandType::COMMENT;
            command.comment = line.substr(1);
            return command;
        }

        // Parse command and arguments
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        // Convert to lowercase for case-insensitive parsing
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        // Parse command type
        if (cmd == "enable" && iss >> cmd && cmd == "matching")
        {
            command.type = ScenarioCommandType::ENABLE_MATCHING;
        }
        else if (cmd == "add" && iss >> cmd)
        {
            if (cmd == "symbol")
            {
                command.type = ScenarioCommandType::ADD_SYMBOL;
            }
            else if (cmd == "book")
            {
                command.type = ScenarioCommandType::ADD_BOOK;
            }
            else if (cmd == "limit")
            {
                iss >> cmd;
                if (cmd == "buy")
                {
                    command.type = ScenarioCommandType::ADD_LIMIT_BUY;
                }
                else if (cmd == "sell")
                {
                    command.type = ScenarioCommandType::ADD_LIMIT_SELL;
                }
            }
            else if (cmd == "market")
            {
                iss >> cmd;
                if (cmd == "buy")
                {
                    command.type = ScenarioCommandType::ADD_MARKET_BUY;
                }
                else if (cmd == "sell")
                {
                    command.type = ScenarioCommandType::ADD_MARKET_SELL;
                }
            }
            else if (cmd == "slippage")
            {
                iss >> cmd;
                if (cmd == "market")
                {
                    iss >> cmd;
                    if (cmd == "buy")
                    {
                        command.type = ScenarioCommandType::ADD_SLIPPAGE_MARKET_BUY;
                    }
                    else if (cmd == "sell")
                    {
                        command.type = ScenarioCommandType::ADD_SLIPPAGE_MARKET_SELL;
                    }
                }
            }
        }
        else if (cmd == "delete")
        {
            iss >> cmd;
            if (cmd == "symbol")
            {
                command.type = ScenarioCommandType::DELETE_SYMBOL;
            }
            else if (cmd == "book")
            {
                command.type = ScenarioCommandType::DELETE_BOOK;
            }
            else if (cmd == "order")
            {
                command.type = ScenarioCommandType::DELETE_ORDER;
            }
        }
        else if (cmd == "reduce")
        {
            command.type = ScenarioCommandType::REDUCE_ORDER;
        }
        else if (cmd == "modify")
        {
            command.type = ScenarioCommandType::MODIFY_ORDER;
        }
        else if (cmd == "replace")
        {
            command.type = ScenarioCommandType::REPLACE_ORDER;
        }
        else
        {
            command.type = ScenarioCommandType::UNKNOWN;
        }

        // Parse arguments
        std::string arg;
        while (iss >> arg)
        {
            command.arguments.push_back(arg);
        }

        return command;
    }

    bool ScenarioRunner::execute_enable_matching(const std::vector<std::string> &)
    {
        matching_enabled_ = true;
        return true;
    }

    bool ScenarioRunner::execute_add_symbol(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 2, "add symbol"))
            return false;

        SymbolId symbol_id = parse_number<SymbolId>(args[0]);
        std::string symbol_name = args[1];

        // Create order book for the symbol
        order_books_.get_order_book(symbol_id);
        return true;
    }

    bool ScenarioRunner::execute_delete_symbol(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 1, "delete symbol"))
            return false;

        (void)parse_number<SymbolId>(args[0]);
        // Note: OrderBookManager doesn't have delete functionality yet
        return true;
    }

    bool ScenarioRunner::execute_add_book(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 1, "add book"))
            return false;

        SymbolId symbol_id = parse_number<SymbolId>(args[0]);
        order_books_.get_order_book(symbol_id);
        return true;
    }

    bool ScenarioRunner::execute_delete_book(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 1, "delete book"))
            return false;

        (void)parse_number<SymbolId>(args[0]);
        // Note: OrderBookManager doesn't have delete functionality yet
        return true;
    }

    bool ScenarioRunner::execute_add_limit_buy(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 5, "add limit buy"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Price price = parse_number<Price>(args[2]);
        Quantity quantity = parse_number<Quantity>(args[3]);

        return order_books_.add_order(symbol_id, order_id, price, quantity, OrderSide::BUY);
    }

    bool ScenarioRunner::execute_add_limit_sell(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 5, "add limit sell"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Price price = parse_number<Price>(args[2]);
        Quantity quantity = parse_number<Quantity>(args[3]);

        return order_books_.add_order(symbol_id, order_id, price, quantity, OrderSide::SELL);
    }

    bool ScenarioRunner::execute_add_market_buy(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 4, "add market buy"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Quantity quantity = parse_number<Quantity>(args[2]);

        // For market orders, we need to execute immediately
        if (matching_enabled_)
        {
            // Execute against best ask
            const OrderBook *order_book = order_books_.get_order_book(symbol_id);
            if (order_book)
            {
                auto [ask_price, ask_qty] = order_book->get_best_ask();
                if (ask_price > 0)
                {
                    order_books_.execute_trade(symbol_id, ask_price, quantity, OrderSide::BUY);
                    position_tracker_.record_trade(symbol_id, ask_price, quantity, OrderSide::BUY, order_id);
                }
            }
        }

        return true;
    }

    bool ScenarioRunner::execute_add_market_sell(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 4, "add market sell"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Quantity quantity = parse_number<Quantity>(args[2]);

        // For market orders, we need to execute immediately
        if (matching_enabled_)
        {
            // Execute against best bid
            const OrderBook *order_book = order_books_.get_order_book(symbol_id);
            if (order_book)
            {
                auto [bid_price, bid_qty] = order_book->get_best_bid();
                if (bid_price > 0)
                {
                    order_books_.execute_trade(symbol_id, bid_price, quantity, OrderSide::SELL);
                    position_tracker_.record_trade(symbol_id, bid_price, quantity, OrderSide::SELL, order_id);
                }
            }
        }

        return true;
    }

    bool ScenarioRunner::execute_reduce_order(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 3, "reduce order"))
            return false;

        (void)parse_number<OrderId>(args[0]);
        (void)parse_number<Quantity>(args[1]);

        // Find the order and reduce it
        // Note: This is simplified - we need to track orders by ID
        return true;
    }

    bool ScenarioRunner::execute_modify_order(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 4, "modify order"))
            return false;

        (void)parse_number<OrderId>(args[0]);
        (void)parse_number<Price>(args[1]);
        (void)parse_number<Quantity>(args[2]);

        // Find the order and modify it
        // Note: This is simplified - we need to track orders by ID
        return true;
    }

    bool ScenarioRunner::execute_replace_order(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 5, "replace order"))
            return false;

        (void)parse_number<OrderId>(args[0]);
        (void)parse_number<OrderId>(args[1]);
        (void)parse_number<Price>(args[2]);
        (void)parse_number<Quantity>(args[3]);

        // Replace the order
        // Note: This is simplified - we need to track orders by ID
        return true;
    }

    bool ScenarioRunner::execute_delete_order(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 1, "delete order"))
            return false;

        (void)parse_number<OrderId>(args[0]);

        // Delete the order
        // Note: This is simplified - we need to track orders by ID
        return true;
    }

    bool ScenarioRunner::execute_add_slippage_market_buy(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 5, "add slippage market buy"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Quantity quantity = parse_number<Quantity>(args[2]);
        Price slippage = parse_number<Price>(args[3]);

        // Execute with slippage
        if (matching_enabled_)
        {
            const OrderBook *order_book = order_books_.get_order_book(symbol_id);
            if (order_book)
            {
                auto [bid_price, bid_qty] = order_book->get_best_bid();
                if (bid_price > 0)
                {
                    Price execution_price = bid_price + slippage;
                    order_books_.execute_trade(symbol_id, execution_price, quantity, OrderSide::BUY);
                    position_tracker_.record_trade(symbol_id, execution_price, quantity, OrderSide::BUY, order_id);
                }
            }
        }

        return true;
    }

    bool ScenarioRunner::execute_add_slippage_market_sell(const std::vector<std::string> &args)
    {
        if (!validate_args(args, 5, "add slippage market sell"))
            return false;

        OrderId order_id = parse_number<OrderId>(args[0]);
        SymbolId symbol_id = parse_number<SymbolId>(args[1]);
        Quantity quantity = parse_number<Quantity>(args[2]);
        Price slippage = parse_number<Price>(args[3]);

        // Execute with slippage
        if (matching_enabled_)
        {
            const OrderBook *order_book = order_books_.get_order_book(symbol_id);
            if (order_book)
            {
                auto [ask_price, ask_qty] = order_book->get_best_ask();
                if (ask_price > 0)
                {
                    Price execution_price = ask_price - slippage;
                    order_books_.execute_trade(symbol_id, execution_price, quantity, OrderSide::SELL);
                    position_tracker_.record_trade(symbol_id, execution_price, quantity, OrderSide::SELL, order_id);
                }
            }
        }

        return true;
    }

    bool ScenarioRunner::validate_args(const std::vector<std::string> &args, size_t expected_count, const std::string &command_name)
    {
        if (args.size() != expected_count)
        {
            std::cerr << "Error: " << command_name << " expects " << expected_count
                      << " arguments, got " << args.size() << std::endl;
            return false;
        }
        return true;
    }

    template <typename T>
    T ScenarioRunner::parse_number(const std::string &str, T default_value)
    {
        try
        {
            if constexpr (std::is_same_v<T, Price>)
            {
                double price_dollars = std::stod(str);
                return price_from_dollars(price_dollars);
            }
            else
            {
                return static_cast<T>(std::stoll(str));
            }
        }
        catch (const std::exception &)
        {
            return default_value;
        }
    }

    template Price ScenarioRunner::parse_number<Price>(const std::string &, Price);
    template Quantity ScenarioRunner::parse_number<Quantity>(const std::string &, Quantity);
    template OrderId ScenarioRunner::parse_number<OrderId>(const std::string &, OrderId);
    template SymbolId ScenarioRunner::parse_number<SymbolId>(const std::string &, SymbolId);

}