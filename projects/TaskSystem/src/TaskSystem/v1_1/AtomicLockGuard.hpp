#pragma once

#include <atomic>
#include <mutex>


namespace std
{

    template <>
    class [[nodiscard]] lock_guard<std::atomic_flag>
    {
    private:
        std::atomic_flag & flag;

    public:
        using mutex_type = std::atomic_flag;

        explicit lock_guard(std::atomic_flag & flag) noexcept : flag(flag)
        {
            // construct and lock
            while (flag.test_and_set(std::memory_order_acquire)) { }
        }

        lock_guard(std::atomic_flag & flag, adopt_lock_t) noexcept : flag(flag)
        { }  // construct but don't lock

        ~lock_guard() noexcept
        {
            flag.clear(std::memory_order_release);
        }

        lock_guard(lock_guard const &) = delete;
        lock_guard & operator=(lock_guard const &) = delete;
    };

    template <>
    class [[nodiscard]] lock_guard<std::atomic<bool>>
    {
    private:
        std::atomic<bool> & flag;

    public:
        using mutex_type = std::atomic<bool>;

        explicit lock_guard(std::atomic<bool> & flag) noexcept : flag(flag)
        {
            // construct and lock
            while (flag.exchange(true, std::memory_order_acquire) != false) { }
        }

        lock_guard(std::atomic<bool> & flag, adopt_lock_t) noexcept : flag(flag)
        { }  // construct but don't lock

        ~lock_guard() noexcept
        {
            flag.store(false, std::memory_order_release);
        }

        lock_guard(lock_guard const &) = delete;
        lock_guard & operator=(lock_guard const &) = delete;
    };

}  // namespace std