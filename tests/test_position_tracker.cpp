#include "position_tracker.hpp"
#include <cassert>
#include <iostream>

using namespace mm;

void test_position_tracker_basic()
{
    std::cout << "Testing basic position tracker functionality..." << std::endl;

    PositionLimits limits;
    limits.max_position_size = 10000;
    limits.max_long_position = 5000;
    limits.max_short_position = 5000;

    PositionTracker tracker(limits);

    assert(tracker.record_trade(1, price_from_dollars(100.00), 1000, OrderSide::BUY, 1));

    const Position *pos = tracker.get_position(1);
    assert(pos != nullptr);
    assert(pos->long_quantity == 1000);
    assert(pos->short_quantity == 0);
    assert(pos->avg_long_price == price_from_dollars(100.00));

    assert(tracker.record_trade(1, price_from_dollars(100.10), 500, OrderSide::SELL, 2));

    pos = tracker.get_position(1);
    assert(pos->long_quantity == 1000);
    assert(pos->short_quantity == 500);
    assert(pos->avg_short_price == price_from_dollars(100.10));

    std::cout << "Basic position tracker test passed!" << std::endl;
}

void test_position_tracker_pnl()
{
    std::cout << "Testing position tracker P&L calculation..." << std::endl;

    PositionLimits limits;
    PositionTracker tracker(limits);

    tracker.record_trade(1, price_from_dollars(100.00), 1000, OrderSide::BUY, 1);

    tracker.record_trade(1, price_from_dollars(100.10), 500, OrderSide::SELL, 2);

    PnL realized_pnl = tracker.get_total_realized_pnl();
    PnL expected_pnl = (price_from_dollars(100.10) - price_from_dollars(100.00)) * 500;
    assert(realized_pnl == expected_pnl);

    tracker.update_unrealized_pnl(1, price_from_dollars(100.05));
    PnL unrealized_pnl = tracker.get_total_unrealized_pnl();

    std::cout << "Position tracker P&L test passed!" << std::endl;
}

int main()
{
    try
    {
        test_position_tracker_basic();
        test_position_tracker_pnl();
        std::cout << "All position tracker tests passed!" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}