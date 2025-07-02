#include "itch_parser.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>

namespace mm
{

    ITCHParser::ITCHParser(OrderBookManager &order_books, PositionTracker &position_tracker)
        : order_books_(order_books), position_tracker_(position_tracker), next_symbol_id_(1)
    {
    }

    bool ITCHParser::parse_file(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open ITCH file: " << filename << std::endl;
            return false;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        const size_t buffer_size = 8192;
        uint8_t buffer[buffer_size];

        while (file.read(reinterpret_cast<char *>(buffer), buffer_size))
        {
            size_t bytes_read = file.gcount();
            size_t offset = 0;

            while (offset < bytes_read)
            {
                if (offset + 2 > bytes_read)
                    break;

                uint16_t message_length = (buffer[offset] << 8) | buffer[offset + 1];
                if (offset + message_length > bytes_read)
                    break;

                if (!process_message(&buffer[offset], message_length))
                {
                    stats_.errors++;
                }

                offset += message_length;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        stats_.processing_time_ms = duration.count();

        return true;
    }

    bool ITCHParser::process_message(const uint8_t *data, size_t length)
    {
        if (length < 3)
            return false;

        (void)((data[0] << 8) | data[1]);
        uint8_t message_type = data[2];

        stats_.total_messages++;

        switch (static_cast<ITCHMessageType>(message_type))
        {
        case ITCHMessageType::ADD_ORDER_NO_MPID:
        case ITCHMessageType::ADD_ORDER_WITH_MPID:
            return parse_add_order(data, length);

        case ITCHMessageType::ORDER_EXECUTED:
        case ITCHMessageType::ORDER_EXECUTED_WITH_PRICE:
            return parse_order_executed(data, length);

        case ITCHMessageType::ORDER_CANCEL:
            return parse_order_cancel(data, length);

        case ITCHMessageType::ORDER_DELETE:
            return parse_order_delete(data, length);

        case ITCHMessageType::ORDER_REPLACE:
            return parse_order_replace(data, length);

        case ITCHMessageType::TRADE:
            return parse_trade(data, length);

        case ITCHMessageType::STOCK_DIRECTORY:
            return parse_stock_directory(data, length);

        default:
            return true;
        }
    }

    bool ITCHParser::parse_add_order(const uint8_t *data, size_t length)
    {
        if (length < 36)
            return false;

        AddOrderMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.order_reference_number = (msg.order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.buy_sell_indicator = data[offset++];
        msg.shares = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.shares = (msg.shares << 8) | data[offset + i];
        }
        offset += 4;

        msg.stock_locate = data[offset++];
        msg.price = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.price = (msg.price << 8) | data[offset + i];
        }
        offset += 4;

        if (msg.type == ITCHMessageType::ADD_ORDER_WITH_MPID)
        {
            std::memcpy(msg.mpid, &data[offset], 4);
            msg.has_mpid = true;
        }
        else
        {
            msg.has_mpid = false;
        }

        SymbolId symbol_id = get_symbol_id(msg.stock_locate);
        Price price = convert_price(msg.price);
        OrderSide side = (msg.buy_sell_indicator == 'B') ? OrderSide::BUY : OrderSide::SELL;

        bool success = order_books_.add_order(symbol_id, msg.order_reference_number,
                                              price, msg.shares, side);

        if (success)
        {
            stats_.add_orders++;
        }

        return success;
    }

    bool ITCHParser::parse_order_executed(const uint8_t *data, size_t length)
    {
        if (length < 32)
            return false;

        OrderExecutedMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.order_reference_number = (msg.order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.executed_shares = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.executed_shares = (msg.executed_shares << 8) | data[offset + i];
        }
        offset += 4;

        msg.match_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.match_number = (msg.match_number << 8) | data[offset + i];
        }

        stats_.executions++;
        return true;
    }

    bool ITCHParser::parse_order_cancel(const uint8_t *data, size_t length)
    {
        if (length < 20)
            return false;

        OrderCancelMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.order_reference_number = (msg.order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.canceled_shares = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.canceled_shares = (msg.canceled_shares << 8) | data[offset + i];
        }

        stats_.cancels++;
        return true;
    }

    bool ITCHParser::parse_order_delete(const uint8_t *data, size_t length)
    {
        if (length < 12)
            return false;

        OrderDeleteMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.order_reference_number = (msg.order_reference_number << 8) | data[offset + i];
        }

        stats_.deletes++;
        return true;
    }

    bool ITCHParser::parse_order_replace(const uint8_t *data, size_t length)
    {
        if (length < 36)
            return false;

        OrderReplaceMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.original_order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.original_order_reference_number = (msg.original_order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.new_order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.new_order_reference_number = (msg.new_order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.shares = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.shares = (msg.shares << 8) | data[offset + i];
        }
        offset += 4;

        msg.price = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.price = (msg.price << 8) | data[offset + i];
        }

        stats_.replaces++;
        return true;
    }

    bool ITCHParser::parse_trade(const uint8_t *data, size_t length)
    {
        if (length < 44)
            return false;

        TradeMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.order_reference_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.order_reference_number = (msg.order_reference_number << 8) | data[offset + i];
        }
        offset += 8;

        msg.buy_sell_indicator = data[offset++];

        msg.shares = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.shares = (msg.shares << 8) | data[offset + i];
        }
        offset += 4;

        msg.stock_locate = data[offset++];

        msg.price = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.price = (msg.price << 8) | data[offset + i];
        }
        offset += 4;

        msg.match_number = 0;
        for (int i = 0; i < 8; i++)
        {
            msg.match_number = (msg.match_number << 8) | data[offset + i];
        }

        SymbolId symbol_id = get_symbol_id(msg.stock_locate);
        Price price = convert_price(msg.price);
        OrderSide side = (msg.buy_sell_indicator == 'B') ? OrderSide::BUY : OrderSide::SELL;

        position_tracker_.record_trade(symbol_id, price, msg.shares, side, msg.order_reference_number);

        stats_.trades++;
        return true;
    }

    bool ITCHParser::parse_stock_directory(const uint8_t *data, size_t length)
    {
        if (length < 40)
            return false;

        StockDirectoryMessage msg;
        msg.type = static_cast<ITCHMessageType>(data[2]);
        msg.length = (data[0] << 8) | data[1];

        size_t offset = 3;

        msg.stock_locate = data[offset++];
        msg.tracking_number = data[offset++];

        std::memcpy(msg.timestamp, &data[offset], 6);
        offset += 6;

        std::memcpy(msg.stock, &data[offset], 8);
        offset += 8;

        msg.market_category = data[offset++];
        msg.financial_status_indicator = data[offset++];

        msg.lot_size = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.lot_size = (msg.lot_size << 8) | data[offset + i];
        }
        offset += 4;

        msg.round_lots_only = data[offset++];
        msg.issue_classification = data[offset++];

        std::memcpy(msg.issue_subtype, &data[offset], 2);
        offset += 2;

        msg.authenticity = data[offset++];
        msg.short_sale_threshold_indicator = data[offset++];
        msg.ipo_flag = data[offset++];
        msg.luld_reference_price_tier = data[offset++];
        msg.etp_flag = data[offset++];

        msg.etp_leverage_factor = 0;
        for (int i = 0; i < 4; i++)
        {
            msg.etp_leverage_factor = (msg.etp_leverage_factor << 8) | data[offset + i];
        }
        offset += 4;

        msg.inverse_indicator = data[offset++];

        get_symbol_id(msg.stock_locate);

        return true;
    }

    SymbolId ITCHParser::get_symbol_id(uint8_t stock_locate)
    {
        auto it = symbol_mapping_.find(stock_locate);
        if (it != symbol_mapping_.end())
        {
            return it->second;
        }

        SymbolId symbol_id = next_symbol_id_++;
        symbol_mapping_[stock_locate] = symbol_id;
        return symbol_id;
    }

    Price ITCHParser::convert_price(uint32_t itch_price)
    {
        return static_cast<Price>(itch_price) * 100;
    }

    Timestamp ITCHParser::convert_timestamp(const uint8_t *itch_timestamp)
    {
        uint64_t nanoseconds = 0;
        for (int i = 0; i < 6; i++)
        {
            nanoseconds = (nanoseconds << 8) | itch_timestamp[i];
        }

        return nanoseconds;
    }

}