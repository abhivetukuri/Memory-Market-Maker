#include "order_book.hpp"
#include <cassert>
#include <iostream>

using namespace mm;

void test_order_book_basic()
{
    std::cout << "Testing basic order book functionality..." << std::endl;

    OrderBook order_book(1);

    assert(order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY));
    assert(order_book.add_order(2, price_from_dollars(100.10), 1000, OrderSide::SELL));

    auto [bid_price, bid_qty] = order_book.get_best_bid();
    auto [ask_price, ask_qty] = order_book.get_best_ask();

    assert(bid_price == price_from_dollars(100.00));
    assert(bid_qty == 1000);
    assert(ask_price == price_from_dollars(100.10));
    assert(ask_qty == 1000);

    Price mid_price = order_book.get_mid_price();
    Price spread = order_book.get_spread();

    assert(mid_price == price_from_dollars(100.05));
    assert(spread == price_from_dollars(0.10));

    std::cout << "Basic order book test passed!" << std::endl;
}

void test_order_book_execution()
{
    std::cout << "Testing order book execution..." << std::endl;

    OrderBook order_book(1);

    order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY);
    order_book.add_order(2, price_from_dollars(100.10), 1000, OrderSide::SELL);

    bool executed = order_book.execute_trade(price_from_dollars(100.00), 500, OrderSide::SELL);
    assert(executed);

    auto [bid_price, bid_qty] = order_book.get_best_bid();
    assert(bid_qty == 500);

    std::cout << "Order book execution test passed!" << std::endl;
}

int main()
{
    try
    {
        test_order_book_basic();
        test_order_book_execution();
        std::cout << "All order book tests passed!" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}