#include "position_tracker.hpp"
#include <algorithm>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace mm
{

    PositionTracker::PositionTracker(const PositionLimits &limits)
        : limits_(limits)
    {
    }

    bool PositionTracker::record_trade(SymbolId symbol, Price price, Quantity quantity, OrderSide side, OrderId order_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Record trade in history
        trade_history_[symbol].emplace_back(symbol, price, quantity, side, order_id);

        // Update position
        update_position(symbol, price, quantity, side);

        // Calculate realized P&L
        PnL realized_pnl = calculate_realized_pnl(symbol, price, quantity, side);
        positions_[symbol].realized_pnl += realized_pnl;

        return true;
    }

    void PositionTracker::update_unrealized_pnl(SymbolId symbol, Price current_price)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = positions_.find(symbol);
        if (it != positions_.end())
        {
            it->second.unrealized_pnl = calculate_unrealized_pnl(it->second, current_price);
            it->second.last_update = get_timestamp();
        }
    }

    void PositionTracker::update_all_unrealized_pnl(const std::map<SymbolId, Price> &current_prices)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto &[symbol, position] : positions_)
        {
            auto price_it = current_prices.find(symbol);
            if (price_it != current_prices.end())
            {
                position.unrealized_pnl = calculate_unrealized_pnl(position, price_it->second);
                position.last_update = get_timestamp();
            }
        }
    }

    const Position *PositionTracker::get_position(SymbolId symbol) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = positions_.find(symbol);
        return (it != positions_.end()) ? &it->second : nullptr;
    }

    std::map<SymbolId, Position> PositionTracker::get_all_positions() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return positions_;
    }

    PnL PositionTracker::get_total_realized_pnl() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        PnL total = 0;
        for (const auto &[symbol, position] : positions_)
        {
            total += position.realized_pnl;
        }
        return total;
    }

    PnL PositionTracker::get_total_unrealized_pnl() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        PnL total = 0;
        for (const auto &[symbol, position] : positions_)
        {
            total += position.unrealized_pnl;
        }
        return total;
    }

    PnL PositionTracker::get_total_pnl() const
    {
        return get_total_realized_pnl() + get_total_unrealized_pnl();
    }

    bool PositionTracker::check_position_limits(SymbolId symbol, Quantity quantity, OrderSide side) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = positions_.find(symbol);
        if (it == positions_.end())
        {
            // New position
            return quantity <= limits_.max_position_size;
        }

        const Position &pos = it->second;
        int64_t net_position = pos.get_net_position();

        if (side == OrderSide::BUY)
        {
            // Check if buying would exceed long limit
            if (net_position + static_cast<int64_t>(quantity) > static_cast<int64_t>(limits_.max_long_position))
            {
                return false;
            }
        }
        else
        {
            // Check if selling would exceed short limit
            if (net_position - static_cast<int64_t>(quantity) < -static_cast<int64_t>(limits_.max_short_position))
            {
                return false;
            }
        }

        // Check total position size
        Quantity new_total = pos.get_total_position() + quantity;
        if (new_total > limits_.max_position_size)
        {
            return false;
        }

        return true;
    }

    bool PositionTracker::check_risk_limits() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        PnL total_pnl = get_total_pnl();

        // Check daily loss limit
        if (total_pnl < -limits_.max_daily_loss)
        {
            return false;
        }

        // Check drawdown limit (simplified - would need historical data for proper calculation)
        if (total_pnl < -limits_.max_drawdown)
        {
            return false;
        }

        return true;
    }

    std::vector<Trade> PositionTracker::get_trade_history(SymbolId symbol) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = trade_history_.find(symbol);
        if (it != trade_history_.end())
        {
            return it->second;
        }
        return {};
    }

    std::vector<Trade> PositionTracker::get_all_trade_history() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Trade> all_trades;
        for (const auto &[symbol, trades] : trade_history_)
        {
            all_trades.insert(all_trades.end(), trades.begin(), trades.end());
        }

        // Sort by timestamp
        std::sort(all_trades.begin(), all_trades.end(),
                  [](const Trade &a, const Trade &b)
                  { return a.timestamp < b.timestamp; });

        return all_trades;
    }

    void PositionTracker::clear_trade_history()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trade_history_.clear();
    }

    PositionTracker::Stats PositionTracker::get_stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Stats stats{
            .total_symbols = positions_.size(),
            .active_positions = 0,
            .total_realized_pnl = 0,
            .total_unrealized_pnl = 0,
            .total_pnl = 0,
            .max_position_size = 0,
            .largest_position_symbol = 0};

        for (const auto &[symbol, position] : positions_)
        {
            if (!position.is_flat())
            {
                stats.active_positions++;
            }

            stats.total_realized_pnl += position.realized_pnl;
            stats.total_unrealized_pnl += position.unrealized_pnl;

            Quantity total_size = position.get_total_position();
            if (total_size > stats.max_position_size)
            {
                stats.max_position_size = total_size;
                stats.largest_position_symbol = symbol;
            }
        }

        stats.total_pnl = stats.total_realized_pnl + stats.total_unrealized_pnl;

        return stats;
    }

    void PositionTracker::reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        positions_.clear();
        trade_history_.clear();
    }

    void PositionTracker::update_position(SymbolId symbol, Price price, Quantity quantity, OrderSide side)
    {
        Position &position = positions_[symbol];
        position.symbol = symbol;
        position.last_update = get_timestamp();

        if (side == OrderSide::BUY)
        {
            // Buying - increase long position
            if (position.long_quantity == 0)
            {
                position.avg_long_price = price;
            }
            else
            {
                // Weighted average price
                Price total_value = position.avg_long_price * position.long_quantity + price * quantity;
                position.avg_long_price = total_value / (position.long_quantity + quantity);
            }
            position.long_quantity += quantity;
        }
        else
        {
            // Selling - increase short position
            if (position.short_quantity == 0)
            {
                position.avg_short_price = price;
            }
            else
            {
                // Weighted average price
                Price total_value = position.avg_short_price * position.short_quantity + price * quantity;
                position.avg_short_price = total_value / (position.short_quantity + quantity);
            }
            position.short_quantity += quantity;
        }
    }

    PnL PositionTracker::calculate_realized_pnl(SymbolId symbol, Price price, Quantity quantity, OrderSide side)
    {
        auto it = positions_.find(symbol);
        if (it == positions_.end())
        {
            return 0; // No existing position
        }

        const Position &position = it->second;
        PnL pnl = 0;

        if (side == OrderSide::BUY)
        {
            // Buying - check if we have short position to cover
            if (position.short_quantity > 0)
            {
                Quantity cover_qty = std::min(quantity, position.short_quantity);
                pnl = (position.avg_short_price - price) * cover_qty; // Profit when covering short
            }
        }
        else
        {
            // Selling - check if we have long position to sell
            if (position.long_quantity > 0)
            {
                Quantity sell_qty = std::min(quantity, position.long_quantity);
                pnl = (price - position.avg_long_price) * sell_qty; // Profit when selling long
            }
        }

        return pnl;
    }

    PnL PositionTracker::calculate_unrealized_pnl(const Position &position, Price current_price) const
    {
        PnL pnl = 0;

        // Unrealized P&L on long position
        if (position.long_quantity > 0)
        {
            pnl += (current_price - position.avg_long_price) * position.long_quantity;
        }

        // Unrealized P&L on short position
        if (position.short_quantity > 0)
        {
            pnl += (position.avg_short_price - current_price) * position.short_quantity;
        }

        return pnl;
    }

    // MMapPositionTracker implementation

    MMapPositionTracker::MMapPositionTracker(const std::string &file_path, const PositionLimits &limits)
        : PositionTracker(limits), file_path_(file_path), mmap_ptr_(nullptr), mmap_size_(0), fd_(-1)
    {
        init_mmap();
        load();
    }

    MMapPositionTracker::~MMapPositionTracker()
    {
        if (mmap_ptr_ != nullptr)
        {
            munmap(mmap_ptr_, mmap_size_);
        }
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    void MMapPositionTracker::flush()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Resize mapping if needed
        size_t required_size = positions_.size() * sizeof(Position);
        if (required_size > mmap_size_)
        {
            resize_mmap(required_size);
        }

        // Copy positions to memory-mapped file
        Position *mmap_positions = static_cast<Position *>(mmap_ptr_);
        size_t i = 0;
        for (const auto &[symbol, position] : positions_)
        {
            mmap_positions[i++] = position;
        }

        // Ensure data is written to disk
        msync(mmap_ptr_, mmap_size_, MS_SYNC);
    }

    void MMapPositionTracker::load()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (mmap_size_ == 0)
        {
            return;
        }

        // Load positions from memory-mapped file
        const Position *mmap_positions = static_cast<const Position *>(mmap_ptr_);
        size_t position_count = mmap_size_ / sizeof(Position);

        positions_.clear();
        for (size_t i = 0; i < position_count; ++i)
        {
            if (mmap_positions[i].symbol != 0)
            { // Valid position
                positions_[mmap_positions[i].symbol] = mmap_positions[i];
            }
        }
    }

    void MMapPositionTracker::init_mmap()
    {
        fd_ = open(file_path_.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd_ == -1)
        {
            throw std::runtime_error("Failed to open position file: " + file_path_);
        }

        // Get file size
        struct stat st;
        if (fstat(fd_, &st) == -1)
        {
            close(fd_);
            throw std::runtime_error("Failed to get file stats");
        }

        mmap_size_ = st.st_size;
        if (mmap_size_ == 0)
        {
            mmap_size_ = 1024 * 1024; // 1MB initial size
            if (ftruncate(fd_, mmap_size_) == -1)
            {
                close(fd_);
                throw std::runtime_error("Failed to resize file");
            }
        }

        // Map file to memory
        mmap_ptr_ = mmap(nullptr, mmap_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED)
        {
            close(fd_);
            throw std::runtime_error("Failed to mmap position file");
        }
    }

    void MMapPositionTracker::resize_mmap(size_t new_size)
    {
        if (mmap_ptr_ != nullptr)
        {
            munmap(mmap_ptr_, mmap_size_);
        }

        if (ftruncate(fd_, new_size) == -1)
        {
            throw std::runtime_error("Failed to resize file");
        }

        mmap_size_ = new_size;
        mmap_ptr_ = mmap(nullptr, mmap_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED)
        {
            throw std::runtime_error("Failed to remap position file");
        }
    }

} // namespace mm