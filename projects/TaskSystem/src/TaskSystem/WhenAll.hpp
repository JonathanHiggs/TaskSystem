#pragma once

#include <TaskSystem/Detail/Promise.hpp>

#include <atomic>
#include <cassert>
#include <coroutine>
#include <memory>


namespace TaskSystem
{
    namespace Detail
    {

        struct WhenAllPromisePolicy
        {
            static inline constexpr bool ScheduleContinuations = true;
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
            static inline constexpr bool AllowSuspendFromCreated = true;
        };

        class WhenAllPromise final : public Promise<void, WhenAllPromisePolicy>
        {
        private:
            std::atomic_size_t count;
            std::coroutine_handle<> resumeHandle;

        public:
            WhenAllPromise(size_t count) noexcept : count(count) { }

            [[nodiscard]] std::coroutine_handle<> Handle() noexcept override
            {
                return resumeHandle;
            }

            // Only allow continuation to be scheduled when the counter ticks to zero
            [[nodiscard]] bool TrySetScheduled() noexcept override
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    return false;
                }

                auto value = count.fetch_sub(1u, std::memory_order_release);

                if (value != 1u)
                {
                    return false;
                }

                state = Completed<>{};
                return false;
            }
        };

        using WhenAllPromisePtr = std::shared_ptr<WhenAllPromise>;

        class WhenAllAwaitable
        {
        private:
            WhenAllPromisePtr promise;

        public:
            WhenAllAwaitable(WhenAllPromisePtr promise) noexcept : promise(std::move(promise))
            {
                // ToDo: assert promise != nullptr
            }

            constexpr bool await_ready() const noexcept { return false; }

            // ToDo: use PromiseType concept
            template <typename TPromise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> callerHandle)
            {
                IPromise & callerPromise = callerHandle.promise();
                return await_suspend(callerHandle, callerPromise);
            }

            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<> callerHandle, IPromise & callerPromise)
            {
                if (promise->State().IsCompleted())
                {
                    return callerHandle;
                }

                if (!callerPromise.TrySetSuspended())
                {
                    assert(false);
                }

                // Capture the current scheduler to ensure the caller is resumed with a scheduler
                promise->ContinuationScheduler(CurrentScheduler());

                if (!promise->TryAddContinuation(callerPromise))
                {
                    assert(false);
                }

                [[maybe_unused]] auto _ = promise->TrySetSuspended();

                return std::noop_coroutine();
            }

            constexpr void await_resume() const noexcept { }
        };

        // Maybe: Should be a schedulable with concept
        template <typename TAwaitable>
        void WhenAllForEach(WhenAllPromisePtr& promise, TAwaitable & awaitable)
        {
            awaitable.ContinueWith(Continuation(*promise, CurrentScheduler()));

            // ToDo: check if awaitable needs scheduling
            // if (awaitable.State() == TaskState::Created && awaitable::CanSchedule)
            // {
            //     awaitable.
            // }
        }

    }  // namespace Detail

    // Maybe: Should be a schedulable with concept
    template <typename ... TAwaitables>
    Detail::WhenAllAwaitable WhenAll(TAwaitables & ... awaitables)
    {
        auto promise = std::make_shared<Detail::WhenAllPromise>(sizeof...(TAwaitables));

        (Detail::WhenAllForEach(promise, awaitables) , ...);

        return Detail::WhenAllAwaitable(std::move(promise));
    }

}  // namespace TaskSystem