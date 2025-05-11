#include "order_book.hpp"
#include <algorithm>
#include <stdexcept>

namespace mm
{

    OrderBook::OrderBook(SymbolId symbol)
        : symbol_(symbol), order_pool_(10000) // Pre-allocate 10K orders
          ,
          level_pool_(1000) // Pre-allocate 1K price levels
    {
    }

    bool OrderBook::add_order(OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if order already exists
        if (orders_.find(order_id) != orders_.end())
        {
            return false;
        }

        // Allocate order from pool
        Order *order = order_pool_.allocate();
        order->id = order_id;
        order->symbol = symbol_;
        order->price = price;
        order->quantity = quantity;
        order->filled_quantity = 0;
        order->side = side;
        order->type = type;
        order->status = OrderStatus::ACTIVE;
        order->timestamp = get_timestamp();

        // Get or create price level
        order->level = get_or_create_level(price, side);

        // Add order to storage
        orders_[order_id] = order;

        // Update level statistics
        update_level_stats(order->level, quantity, true);

        return true;
    }

    bool OrderBook::cancel_order(OrderId order_id, Quantity quantity)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Order *order = find_order(order_id);
        if (!order || order->status != OrderStatus::ACTIVE)
        {
            return false;
        }

        Quantity cancel_qty = (quantity == 0) ? order->quantity - order->filled_quantity : quantity;
        if (cancel_qty > order->quantity - order->filled_quantity)
        {
            cancel_qty = order->quantity - order->filled_quantity;
        }

        // Update order
        order->filled_quantity += cancel_qty;
        if (order->filled_quantity >= order->quantity)
        {
            order->status = OrderStatus::FILLED;
        }

        // Update level statistics
        update_level_stats(order->level, cancel_qty, false);

        // Remove order if fully filled or cancelled
        if (order->status == OrderStatus::FILLED)
        {
            remove_order_from_level(order);
            orders_.erase(order_id);
            order_pool_.deallocate(order);
        }

        return true;
    }

    bool OrderBook::modify_order(OrderId order_id, Price new_price, Quantity new_quantity)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Order *order = find_order(order_id);
        if (!order || order->status != OrderStatus::ACTIVE)
        {
            return false;
        }

        // Remove from current level
        Quantity old_remaining = order->quantity - order->filled_quantity;
        update_level_stats(order->level, old_remaining, false);

        // Update order
        order->price = new_price;
        order->quantity = new_quantity;
        order->timestamp = get_timestamp();

        // Add to new level
        order->level = get_or_create_level(new_price, order->side);
        update_level_stats(order->level, new_quantity - order->filled_quantity, true);

        return true;
    }

    bool OrderBook::execute_trade(Price price, Quantity quantity, OrderSide side)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Quantity remaining_qty = quantity;

        if (side == OrderSide::BUY)
        {
            // Buying - execute against asks
            if (asks_.empty())
            {
                return false;
            }

            // Execute against resting orders
            for (auto it = asks_.begin(); it != asks_.end() && remaining_qty > 0;)
            {
                PriceLevel *level = it->second;

                // Check if price is acceptable
                if (level->price > price)
                {
                    break;
                }

                // Execute against this level
                Quantity execute_qty = std::min(remaining_qty, level->total_quantity);
                level->total_quantity -= execute_qty;
                remaining_qty -= execute_qty;

                // Update orders at this level
                for (auto &order_pair : orders_)
                {
                    Order *order = order_pair.second;
                    if (order->level == level && order->status == OrderStatus::ACTIVE)
                    {
                        Quantity order_execute = std::min(execute_qty, order->quantity - order->filled_quantity);
                        order->filled_quantity += order_execute;
                        execute_qty -= order_execute;

                        if (order->filled_quantity >= order->quantity)
                        {
                            order->status = OrderStatus::FILLED;
                            remove_order_from_level(order);
                            order_pool_.deallocate(order);
                        }

                        if (execute_qty == 0)
                            break;
                    }
                }

                // Remove empty level
                if (level->total_quantity == 0)
                {
                    it = asks_.erase(it);
                    level_pool_.deallocate(level);
                }
                else
                {
                    ++it;
                }
            }
        }
        else
        {
            // Selling - execute against bids
            if (bids_.empty())
            {
                return false;
            }

            // Execute against resting orders
            for (auto it = bids_.begin(); it != bids_.end() && remaining_qty > 0;)
            {
                PriceLevel *level = it->second;

                // Check if price is acceptable
                if (level->price < price)
                {
                    break;
                }

                // Execute against this level
                Quantity execute_qty = std::min(remaining_qty, level->total_quantity);
                level->total_quantity -= execute_qty;
                remaining_qty -= execute_qty;

                // Update orders at this level
                for (auto &order_pair : orders_)
                {
                    Order *order = order_pair.second;
                    if (order->level == level && order->status == OrderStatus::ACTIVE)
                    {
                        Quantity order_execute = std::min(execute_qty, order->quantity - order->filled_quantity);
                        order->filled_quantity += order_execute;
                        execute_qty -= order_execute;

                        if (order->filled_quantity >= order->quantity)
                        {
                            order->status = OrderStatus::FILLED;
                            remove_order_from_level(order);
                            order_pool_.deallocate(order);
                        }

                        if (execute_qty == 0)
                            break;
                    }
                }

                // Remove empty level
                if (level->total_quantity == 0)
                {
                    it = bids_.erase(it);
                    level_pool_.deallocate(level);
                }
                else
                {
                    ++it;
                }
            }
        }

        return remaining_qty < quantity; // Return true if any execution occurred
    }

    std::pair<Price, Quantity> OrderBook::get_best_bid() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (bids_.empty())
        {
            return {0, 0};
        }

        const PriceLevel *level = bids_.begin()->second;
        return {level->price, level->total_quantity};
    }

    std::pair<Price, Quantity> OrderBook::get_best_ask() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (asks_.empty())
        {
            return {0, 0};
        }

        const PriceLevel *level = asks_.begin()->second;
        return {level->price, level->total_quantity};
    }

    Price OrderBook::get_mid_price() const
    {
        auto [bid_price, bid_qty] = get_best_bid();
        auto [ask_price, ask_qty] = get_best_ask();

        if (bid_price == 0 || ask_price == 0)
        {
            return 0;
        }

        return (bid_price + ask_price) / 2;
    }

    Price OrderBook::get_spread() const
    {
        auto [bid_price, bid_qty] = get_best_bid();
        auto [ask_price, ask_qty] = get_best_ask();

        if (bid_price == 0 || ask_price == 0)
        {
            return 0;
        }

        return ask_price - bid_price;
    }

    std::vector<std::pair<Price, Quantity>> OrderBook::get_bids(size_t depth) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::pair<Price, Quantity>> result;
        result.reserve(std::min(depth, bids_.size()));

        size_t count = 0;
        for (const auto &[price, level] : bids_)
        {
            if (count >= depth)
                break;
            result.emplace_back(level->price, level->total_quantity);
            ++count;
        }

        return result;
    }

    std::vector<std::pair<Price, Quantity>> OrderBook::get_asks(size_t depth) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::pair<Price, Quantity>> result;
        result.reserve(std::min(depth, asks_.size()));

        size_t count = 0;
        for (const auto &[price, level] : asks_)
        {
            if (count >= depth)
                break;
            result.emplace_back(level->price, level->total_quantity);
            ++count;
        }

        return result;
    }

    const Order *OrderBook::get_order(OrderId order_id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = orders_.find(order_id);
        return (it != orders_.end()) ? it->second : nullptr;
    }

    OrderBook::Stats OrderBook::get_stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto [best_bid_price, best_bid_qty] = get_best_bid();
        auto [best_ask_price, best_ask_qty] = get_best_ask();

        return Stats{
            .total_orders = orders_.size(),
            .active_orders = static_cast<size_t>(std::count_if(orders_.begin(), orders_.end(),
                                                               [](const auto &p)
                                                               { return p.second->status == OrderStatus::ACTIVE; })),
            .bid_levels = bids_.size(),
            .ask_levels = asks_.size(),
            .best_bid = best_bid_price,
            .best_ask = best_ask_price,
            .mid_price = get_mid_price(),
            .spread = get_spread()};
    }

    PriceLevel *OrderBook::get_or_create_level(Price price, OrderSide side)
    {
        if (side == OrderSide::BUY)
        {
            auto it = bids_.find(price);
            if (it != bids_.end())
            {
                return it->second;
            }

            // Create new level
            PriceLevel *level = level_pool_.allocate();
            level->price = price;
            level->total_quantity = 0;
            level->order_count = 0;
            level->last_update = get_timestamp();

            bids_[price] = level;
            return level;
        }
        else
        {
            auto it = asks_.find(price);
            if (it != asks_.end())
            {
                return it->second;
            }

            // Create new level
            PriceLevel *level = level_pool_.allocate();
            level->price = price;
            level->total_quantity = 0;
            level->order_count = 0;
            level->last_update = get_timestamp();

            asks_[price] = level;
            return level;
        }
    }

    void OrderBook::remove_empty_level(Price price, OrderSide side)
    {
        if (side == OrderSide::BUY)
        {
            auto it = bids_.find(price);
            if (it != bids_.end() && it->second->total_quantity == 0)
            {
                level_pool_.deallocate(it->second);
                bids_.erase(it);
            }
        }
        else
        {
            auto it = asks_.find(price);
            if (it != asks_.end() && it->second->total_quantity == 0)
            {
                level_pool_.deallocate(it->second);
                asks_.erase(it);
            }
        }
    }

    void OrderBook::update_level_stats(PriceLevel *level, Quantity delta, bool add_order)
    {
        if (add_order)
        {
            level->total_quantity += delta;
            level->order_count++;
        }
        else
        {
            level->total_quantity -= delta;
            level->order_count--;
        }
        level->last_update = get_timestamp();
    }

    Order *OrderBook::find_order(OrderId order_id)
    {
        auto it = orders_.find(order_id);
        return (it != orders_.end()) ? it->second : nullptr;
    }

    void OrderBook::remove_order_from_level(Order *order)
    {
        if (order->level)
        {
            update_level_stats(order->level, order->quantity - order->filled_quantity, false);
            remove_empty_level(order->level->price, order->side);
            order->level = nullptr;
        }
    }

    // OrderBookManager implementation

    OrderBookManager::OrderBookManager() = default;

    OrderBook *OrderBookManager::get_order_book(SymbolId symbol)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = order_books_.find(symbol);
        if (it != order_books_.end())
        {
            return it->second.get();
        }

        // Create new order book
        auto order_book = std::make_unique<OrderBook>(symbol);
        OrderBook *ptr = order_book.get();
        order_books_[symbol] = std::move(order_book);
        return ptr;
    }

    bool OrderBookManager::add_order(SymbolId symbol, OrderId order_id, Price price, Quantity quantity, OrderSide side, OrderType type)
    {
        OrderBook *order_book = get_order_book(symbol);
        return order_book->add_order(order_id, price, quantity, side, type);
    }

    bool OrderBookManager::cancel_order(SymbolId symbol, OrderId order_id, Quantity quantity)
    {
        const OrderBook *order_book = get_order_book(symbol);
        if (!order_book)
            return false;

        // Need to cast away const for modification
        return const_cast<OrderBook *>(order_book)->cancel_order(order_id, quantity);
    }

    bool OrderBookManager::modify_order(SymbolId symbol, OrderId order_id, Price new_price, Quantity new_quantity)
    {
        OrderBook *order_book = get_order_book(symbol);
        if (!order_book)
            return false;

        return order_book->modify_order(order_id, new_price, new_quantity);
    }

    bool OrderBookManager::execute_trade(SymbolId symbol, Price price, Quantity quantity, OrderSide side)
    {
        OrderBook *order_book = get_order_book(symbol);
        if (!order_book)
            return false;

        return order_book->execute_trade(price, quantity, side);
    }

    const OrderBook *OrderBookManager::get_order_book(SymbolId symbol) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = order_books_.find(symbol);
        return (it != order_books_.end()) ? it->second.get() : nullptr;
    }

    std::vector<SymbolId> OrderBookManager::get_active_symbols() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<SymbolId> symbols;
        symbols.reserve(order_books_.size());

        for (const auto &[symbol, order_book] : order_books_)
        {
            symbols.push_back(symbol);
        }

        return symbols;
    }

} // namespace mm