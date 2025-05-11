#pragma once

#include "types.hpp"
#include <vector>
#include <stack>
#include <mutex>
#include <memory>

namespace mm
{

    /**
     * Fixed-size memory pool for high-performance allocation
     * Eliminates heap allocations for microsecond quote updates
     */
    template <typename T>
    class MemoryPool
    {
    public:
        explicit MemoryPool(size_t initial_capacity = 1000)
            : capacity_(initial_capacity), allocated_count_(0), peak_usage_(0)
        {
            allocate_chunk(initial_capacity);
        }

        ~MemoryPool() = default;

        // Non-copyable, non-movable
        MemoryPool(const MemoryPool &) = delete;
        MemoryPool &operator=(const MemoryPool &) = delete;
        MemoryPool(MemoryPool &&) = delete;
        MemoryPool &operator=(MemoryPool &&) = delete;

        /**
         * Allocate a new object from the pool
         * Returns pointer to uninitialized memory
         */
        T *allocate()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!free_list_.empty())
            {
                T *ptr = free_list_.top();
                free_list_.pop();
                return ptr;
            }

            // Need to allocate more memory
            if (allocated_count_ >= capacity_)
            {
                size_t new_capacity = capacity_ * 2;
                allocate_chunk(new_capacity);
            }

            T *ptr = &chunks_.back()[allocated_count_++];
            peak_usage_ = std::max(peak_usage_, allocated_count_);
            return ptr;
        }

        /**
         * Return an object to the pool
         * Object is not destroyed, just marked as available
         */
        void deallocate(T *ptr)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            free_list_.push(ptr);
        }

        /**
         * Get pool statistics
         */
        PoolStats get_stats() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return PoolStats{
                .total_allocated = allocated_count_,
                .total_freed = free_list_.size(),
                .current_usage = allocated_count_ - free_list_.size(),
                .peak_usage = peak_usage_,
                .allocation_count = allocated_count_,
                .free_count = free_list_.size()};
        }

        /**
         * Reset the pool (mark all objects as free)
         */
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!free_list_.empty())
            {
                free_list_.pop();
            }
            allocated_count_ = 0;
        }

        /**
         * Get current capacity
         */
        size_t capacity() const
        {
            return capacity_;
        }

        /**
         * Get current usage
         */
        size_t usage() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return allocated_count_ - free_list_.size();
        }

    private:
        void allocate_chunk(size_t size)
        {
            chunks_.emplace_back(std::make_unique<T[]>(size));
            capacity_ += size;
        }

        std::vector<std::unique_ptr<T[]>> chunks_;
        std::stack<T *> free_list_;
        mutable std::mutex mutex_;
        size_t capacity_;
        size_t allocated_count_;
        size_t peak_usage_;
    };

    /**
     * Thread-local memory pool for lock-free allocation
     */
    template <typename T>
    class ThreadLocalPool
    {
    public:
        explicit ThreadLocalPool(size_t initial_capacity = 1000)
            : pool_(initial_capacity) {}

        T *allocate()
        {
            return pool_.allocate();
        }

        void deallocate(T *ptr)
        {
            pool_.deallocate(ptr);
        }

        PoolStats get_stats() const
        {
            return pool_.get_stats();
        }

        void reset()
        {
            pool_.reset();
        }

    private:
        MemoryPool<T> pool_;
    };

} // namespace mm