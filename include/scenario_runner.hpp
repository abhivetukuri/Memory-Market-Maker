#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include "position_tracker.hpp"
#include <string>
#include <vector>
#include <map>

namespace mm
{

    /**
     * Scenario command types
     */
    enum class ScenarioCommandType
    {
        ENABLE_MATCHING,
        ADD_SYMBOL,
        DELETE_SYMBOL,
        ADD_BOOK,
        DELETE_BOOK,
        ADD_LIMIT_BUY,
        ADD_LIMIT_SELL,
        ADD_MARKET_BUY,
        ADD_MARKET_SELL,
        REDUCE_ORDER,
        MODIFY_ORDER,
        REPLACE_ORDER,
        DELETE_ORDER,
        ADD_SLIPPAGE_MARKET_BUY,
        ADD_SLIPPAGE_MARKET_SELL,
        COMMENT,
        UNKNOWN
    };

    /**
     * Scenario command structure
     */
    struct ScenarioCommand
    {
        ScenarioCommandType type;
        std::vector<std::string> arguments;
        std::string comment;
        size_t line_number;
    };

    /**
     * Scenario test result
     */
    struct ScenarioResult
    {
        std::string scenario_name;
        bool passed;
        std::string error_message;
        double execution_time_ms;
        size_t orders_processed;
        size_t trades_executed;

        // Final state
        std::map<SymbolId, OrderBook::Stats> order_book_stats;
        PositionTracker::Stats position_stats;
    };

    /**
     * Scenario runner for testing order book and matching engine
     */
    class ScenarioRunner
    {
    public:
        explicit ScenarioRunner(OrderBookManager &order_books, PositionTracker &position_tracker);
        ~ScenarioRunner() = default;

        // Non-copyable, non-movable
        ScenarioRunner(const ScenarioRunner &) = delete;
        ScenarioRunner &operator=(const ScenarioRunner &) = delete;
        ScenarioRunner(ScenarioRunner &&) = delete;
        ScenarioRunner &operator=(ScenarioRunner &&) = delete;

        /**
         * Run a single scenario file
         */
        ScenarioResult run_scenario(const std::string &filename);

        /**
         * Run all scenarios in a directory
         */
        std::vector<ScenarioResult> run_all_scenarios(const std::string &directory);

        /**
         * Run a specific scenario by name
         */
        ScenarioResult run_scenario_by_name(const std::string &scenario_name);

        /**
         * Get scenario statistics
         */
        struct Stats
        {
            size_t total_scenarios;
            size_t passed_scenarios;
            size_t failed_scenarios;
            double total_execution_time_ms;
            double avg_execution_time_ms;
        };

        Stats get_stats() const { return stats_; }

        /**
         * Reset statistics
         */
        void reset_stats() { stats_ = Stats{}; }

        /**
         * Enable/disable matching engine
         */
        void set_matching_enabled(bool enabled) { matching_enabled_ = enabled; }

        /**
         * Get matching engine status
         */
        bool is_matching_enabled() const { return matching_enabled_; }

    private:
        OrderBookManager &order_books_;
        PositionTracker &position_tracker_;
        Stats stats_;
        bool matching_enabled_;

        /**
         * Parse scenario file into commands
         */
        std::vector<ScenarioCommand> parse_scenario_file(const std::string &filename);

        /**
         * Execute a single scenario command
         */
        bool execute_command(const ScenarioCommand &command);

        /**
         * Parse command line
         */
        ScenarioCommand parse_command_line(const std::string &line, size_t line_number);

        /**
         * Execute specific command types
         */
        bool execute_enable_matching(const std::vector<std::string> &args);
        bool execute_add_symbol(const std::vector<std::string> &args);
        bool execute_delete_symbol(const std::vector<std::string> &args);
        bool execute_add_book(const std::vector<std::string> &args);
        bool execute_delete_book(const std::vector<std::string> &args);
        bool execute_add_limit_buy(const std::vector<std::string> &args);
        bool execute_add_limit_sell(const std::vector<std::string> &args);
        bool execute_add_market_buy(const std::vector<std::string> &args);
        bool execute_add_market_sell(const std::vector<std::string> &args);
        bool execute_reduce_order(const std::vector<std::string> &args);
        bool execute_modify_order(const std::vector<std::string> &args);
        bool execute_replace_order(const std::vector<std::string> &args);
        bool execute_delete_order(const std::vector<std::string> &args);
        bool execute_add_slippage_market_buy(const std::vector<std::string> &args);
        bool execute_add_slippage_market_sell(const std::vector<std::string> &args);

        /**
         * Validate command arguments
         */
        bool validate_args(const std::vector<std::string> &args, size_t expected_count, const std::string &command_name);

        /**
         * Convert string to number with error handling
         */
        template <typename T>
        T parse_number(const std::string &str, T default_value = 0);
    };

} // namespace mm