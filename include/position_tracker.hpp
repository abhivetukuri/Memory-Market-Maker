#pragma once

#include "types.hpp"
#include <map>
#include <memory>
#include <shared_mutex>

namespace mm
{

    /**
     * Position information for a single symbol
     */
    struct alignas(ALIGNMENT) Position
    {
        SymbolId symbol;
        Quantity long_quantity;
        Quantity short_quantity;
        Price avg_long_price;
        Price avg_short_price;
        PnL realized_pnl;
        PnL unrealized_pnl;
        Timestamp last_update;

        Position() : symbol(0), long_quantity(0), short_quantity(0),
                     avg_long_price(0), avg_short_price(0), realized_pnl(0),
                     unrealized_pnl(0), last_update(0) {}

        /**
         * Get net position (positive = long, negative = short)
         */
        int64_t get_net_position() const
        {
            return static_cast<int64_t>(long_quantity) - static_cast<int64_t>(short_quantity);
        }

        /**
         * Get total position size
         */
        Quantity get_total_position() const
        {
            return long_quantity + short_quantity;
        }

        /**
         * Check if position is flat
         */
        bool is_flat() const
        {
            return long_quantity == 0 && short_quantity == 0;
        }

        /**
         * Check if position is long
         */
        bool is_long() const
        {
            return long_quantity > short_quantity;
        }

        /**
         * Check if position is short
         */
        bool is_short() const
        {
            return short_quantity > long_quantity;
        }
    };

    /**
     * Trade record for P&L calculation
     */
    struct Trade
    {
        SymbolId symbol;
        Price price;
        Quantity quantity;
        OrderSide side;
        Timestamp timestamp;
        OrderId order_id;

        Trade() : symbol(0), price(0), quantity(0), side(OrderSide::BUY), timestamp(0), order_id(0) {}
        Trade(SymbolId s, Price p, Quantity q, OrderSide sd, OrderId oid)
            : symbol(s), price(p), quantity(q), side(sd), timestamp(get_timestamp()), order_id(oid) {}
    };

    /**
     * Position limits and risk management
     */
    struct PositionLimits
    {
        Quantity max_position_size;
        Quantity max_long_position;
        Quantity max_short_position;
        PnL max_daily_loss;
        PnL max_drawdown;

        PositionLimits() : max_position_size(1000000), max_long_position(500000),
                           max_short_position(500000), max_daily_loss(1000000), max_drawdown(500000) {}
    };

    /**
     * High-performance position tracker with real-time P&L calculation
     */
    class PositionTracker
    {
    public:
        explicit PositionTracker(const PositionLimits &limits = PositionLimits());
        ~PositionTracker() = default;

        // Non-copyable, non-movable
        PositionTracker(const PositionTracker &) = delete;
        PositionTracker &operator=(const PositionTracker &) = delete;
        PositionTracker(PositionTracker &&) = delete;
        PositionTracker &operator=(PositionTracker &&) = delete;

        /**
         * Record a trade and update position
         */
        bool record_trade(SymbolId symbol, Price price, Quantity quantity, OrderSide side, OrderId order_id);

        /**
         * Update unrealized P&L based on current market prices
         */
        void update_unrealized_pnl(SymbolId symbol, Price current_price);

        /**
         * Update unrealized P&L for all positions
         */
        void update_all_unrealized_pnl(const std::map<SymbolId, Price> &current_prices);

        /**
         * Get position for a symbol
         */
        const Position *get_position(SymbolId symbol) const;

        /**
         * Get all positions
         */
        std::map<SymbolId, Position> get_all_positions() const;

        /**
         * Get total realized P&L across all symbols
         */
        PnL get_total_realized_pnl() const;

        /**
         * Get total unrealized P&L across all symbols
         */
        PnL get_total_unrealized_pnl() const;

        /**
         * Get total P&L (realized + unrealized)
         */
        PnL get_total_pnl() const;

        /**
         * Check if position limits are exceeded
         */
        bool check_position_limits(SymbolId symbol, Quantity quantity, OrderSide side) const;

        /**
         * Check if risk limits are exceeded
         */
        bool check_risk_limits() const;

        /**
         * Get position limits
         */
        const PositionLimits &get_limits() const { return limits_; }

        /**
         * Set position limits
         */
        void set_limits(const PositionLimits &limits) { limits_ = limits; }

        /**
         * Get trade history for a symbol
         */
        std::vector<Trade> get_trade_history(SymbolId symbol) const;

        /**
         * Get all trade history
         */
        std::vector<Trade> get_all_trade_history() const;

        /**
         * Clear trade history (for memory management)
         */
        void clear_trade_history();

        /**
         * Get position statistics
         */
        struct Stats
        {
            size_t total_symbols;
            size_t active_positions;
            PnL total_realized_pnl;
            PnL total_unrealized_pnl;
            PnL total_pnl;
            Quantity max_position_size;
            SymbolId largest_position_symbol;
        };

        Stats get_stats() const;

        /**
         * Reset all positions and P&L
         */
        void reset();

    protected:
        std::map<SymbolId, Position> positions_;
        std::map<SymbolId, std::vector<Trade>> trade_history_;
        PositionLimits limits_;
        mutable std::mutex mutex_;

        /**
         * Update position for a trade
         */
        void update_position(SymbolId symbol, Price price, Quantity quantity, OrderSide side);

        /**
         * Calculate realized P&L for a trade
         */
        PnL calculate_realized_pnl(SymbolId symbol, Price price, Quantity quantity, OrderSide side);

        /**
         * Calculate unrealized P&L for a position
         */
        PnL calculate_unrealized_pnl(const Position &position, Price current_price) const;
    };

    /**
     * Memory-mapped position tracker for persistence
     */
    class MMapPositionTracker : public PositionTracker
    {
    public:
        explicit MMapPositionTracker(const std::string &file_path, const PositionLimits &limits = PositionLimits());
        ~MMapPositionTracker();

        /**
         * Flush positions to memory-mapped file
         */
        void flush();

        /**
         * Load positions from memory-mapped file
         */
        void load();

    private:
        std::string file_path_;
        void *mmap_ptr_;
        size_t mmap_size_;
        int fd_;

        /**
         * Initialize memory mapping
         */
        void init_mmap();

        /**
         * Resize memory mapping if needed
         */
        void resize_mmap(size_t new_size);
    };

} // namespace mm