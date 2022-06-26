#pragma once

#include <TaskSystem/Detail/Promise.hpp>
#include <TaskSystem/ValueTask.hpp>

#include <atomic>
#include <cassert>
#include <coroutine>
#include <memory>


namespace TaskSystem
{
    namespace Detail
    {

        struct WhenAnyPromisePolicy
        {
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
            static inline constexpr bool AllowSuspendFromCreated = true;
        };

        class WhenAnyPromise final : public Promise<void, WhenAnyPromisePolicy>
        {
        private:
            std::atomic_bool resumed;

        public:
            WhenAnyPromise() noexcept : resumed(false) { }

            [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

            [[nodiscard]] bool TrySetScheduled() noexcept override
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    return false;
                }

                CheckResume();
                return false;
            }

            void CheckResume() noexcept
            {
                auto result = resumed.exchange(true, std::memory_order_acquire);
            }
        };

        using WhenAnyPromisePtr = std::shared_ptr<WhenAnyPromise>;

        class WhenAnyAwaitable
        {
        private:
            WhenAnyPromisePtr promise;

        public:
            WhenAnyAwaitable(WhenAnyPromisePtr promise) noexcept : promise(std::move(promise))
            {}

        };

    }
}