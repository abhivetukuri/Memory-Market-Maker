#include "order_book.hpp"
#include "types.hpp"
#include <iostream>

using namespace mm;

int main()
{
    std::cout << "Debug test - OrderBook stats" << std::endl;

    OrderBook order_book(1);

    std::cout << "Created order book" << std::endl;

    // Add a simple order
    order_book.add_order(1, price_from_dollars(100.00), 1000, OrderSide::BUY);
    std::cout << "Added order" << std::endl;

    // Try to get stats
    std::cout << "Getting stats..." << std::endl;
    auto stats = order_book.get_stats();
    std::cout << "Got stats successfully!" << std::endl;

    std::cout << "Total Orders: " << stats.total_orders << std::endl;

    return 0;
}