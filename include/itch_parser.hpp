#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include "position_tracker.hpp"
#include <fstream>
#include <vector>
#include <memory>

namespace mm
{

    /**
     * ITCH message types
     */
    enum class ITCHMessageType : uint8_t
    {
        SYSTEM_EVENT = 'S',
        STOCK_DIRECTORY = 'R',
        STOCK_TRADING_ACTION = 'H',
        REG_SHO_RESTRICTION = 'Y',
        MARKET_PARTICIPANT_POSITION = 'L',
        MWCB_DECLINE_LEVEL = 'V',
        MWCB_STATUS = 'W',
        IPO_QUOTING_PERIOD_UPDATE = 'K',
        LULD_AUCTION_COLLAR = 'J',
        OPERATIONAL_HALT = 'h',
        ADD_ORDER_NO_MPID = 'A',
        ADD_ORDER_WITH_MPID = 'F',
        ORDER_EXECUTED = 'E',
        ORDER_EXECUTED_WITH_PRICE = 'C',
        ORDER_CANCEL = 'X',
        ORDER_DELETE = 'D',
        ORDER_REPLACE = 'U',
        TRADE = 'P',
        CROSS_TRADE = 'Q',
        BROKEN_TRADE = 'B',
        NOII = 'I',
        RPII = 'N'
    };

    /**
     * ITCH message base class
     */
    struct ITCHMessage
    {
        ITCHMessageType type;
        uint16_t length;
        uint64_t timestamp;

        virtual ~ITCHMessage() = default;
    };

    /**
     * Add Order message
     */
    struct AddOrderMessage : public ITCHMessage
    {
        uint64_t order_reference_number;
        uint8_t buy_sell_indicator;
        uint32_t shares;
        uint8_t stock_locate;
        uint32_t price;
        uint8_t mpid[4];
        bool has_mpid;
    };

    /**
     * Order Executed message
     */
    struct OrderExecutedMessage : public ITCHMessage
    {
        uint64_t order_reference_number;
        uint32_t executed_shares;
        uint64_t match_number;
    };

    /**
     * Order Cancel message
     */
    struct OrderCancelMessage : public ITCHMessage
    {
        uint64_t order_reference_number;
        uint32_t canceled_shares;
    };

    /**
     * Order Delete message
     */
    struct OrderDeleteMessage : public ITCHMessage
    {
        uint64_t order_reference_number;
    };

    /**
     * Order Replace message
     */
    struct OrderReplaceMessage : public ITCHMessage
    {
        uint64_t original_order_reference_number;
        uint64_t new_order_reference_number;
        uint32_t shares;
        uint32_t price;
    };

    /**
     * Trade message
     */
    struct TradeMessage : public ITCHMessage
    {
        uint64_t order_reference_number;
        uint8_t buy_sell_indicator;
        uint32_t shares;
        uint8_t stock_locate;
        uint32_t price;
        uint64_t match_number;
    };

    /**
     * Stock Directory message
     */
    struct StockDirectoryMessage : public ITCHMessage
    {
        uint8_t stock_locate;
        uint8_t tracking_number;
        uint8_t timestamp[6];
        char stock[8];
        char market_category;
        char financial_status_indicator;
        uint32_t lot_size;
        char round_lots_only;
        char issue_classification;
        char issue_subtype[2];
        char authenticity;
        char short_sale_threshold_indicator;
        char ipo_flag;
        char luld_reference_price_tier;
        char etp_flag;
        uint32_t etp_leverage_factor;
        char inverse_indicator;
    };

    /**
     * High-performance ITCH parser for NASDAQ TotalView-ITCH data
     */
    class ITCHParser
    {
    public:
        explicit ITCHParser(OrderBookManager &order_books, PositionTracker &position_tracker);
        ~ITCHParser() = default;

        // Non-copyable, non-movable
        ITCHParser(const ITCHParser &) = delete;
        ITCHParser &operator=(const ITCHParser &) = delete;
        ITCHParser(ITCHParser &&) = delete;
        ITCHParser &operator=(ITCHParser &&) = delete;

        /**
         * Parse ITCH file and process all messages
         */
        bool parse_file(const std::string &filename);

        /**
         * Process a single ITCH message
         */
        bool process_message(const uint8_t *data, size_t length);

        /**
         * Get parsing statistics
         */
        struct Stats
        {
            uint64_t total_messages;
            uint64_t add_orders;
            uint64_t executions;
            uint64_t cancels;
            uint64_t deletes;
            uint64_t replaces;
            uint64_t trades;
            uint64_t errors;
            double processing_time_ms;
        };

        Stats get_stats() const { return stats_; }

        /**
         * Reset statistics
         */
        void reset_stats() { stats_ = Stats{}; }

    private:
        OrderBookManager &order_books_;
        PositionTracker &position_tracker_;
        Stats stats_;

        // Symbol mapping (stock_locate -> symbol_id)
        std::map<uint8_t, SymbolId> symbol_mapping_;
        SymbolId next_symbol_id_;

        /**
         * Parse specific message types
         */
        bool parse_add_order(const uint8_t *data, size_t length);
        bool parse_order_executed(const uint8_t *data, size_t length);
        bool parse_order_cancel(const uint8_t *data, size_t length);
        bool parse_order_delete(const uint8_t *data, size_t length);
        bool parse_order_replace(const uint8_t *data, size_t length);
        bool parse_trade(const uint8_t *data, size_t length);
        bool parse_stock_directory(const uint8_t *data, size_t length);

        /**
         * Get or create symbol mapping
         */
        SymbolId get_symbol_id(uint8_t stock_locate);

        /**
         * Convert ITCH price to internal price format
         */
        Price convert_price(uint32_t itch_price);

        /**
         * Convert ITCH timestamp to internal timestamp
         */
        Timestamp convert_timestamp(const uint8_t *itch_timestamp);
    };

} // namespace mm