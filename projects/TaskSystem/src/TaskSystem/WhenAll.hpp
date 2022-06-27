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

        struct WhenAllPromisePolicy
        {
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
            static inline constexpr bool AllowSuspendFromCreated = true;
        };

        class WhenAllPromise final : public Promise<void, WhenAllPromisePolicy>
        {
        private:
            std::atomic_size_t count;

        public:
            WhenAllPromise(size_t count) noexcept : count(count) { }

            [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

            [[nodiscard]] SetScheduledResult TrySetScheduled() noexcept override
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    if (StateIsOneOf<Running>())
                    {
                        return SetScheduledError::PromiseRunning;
                    }
                    else if (StateIsOneOf<Completed<void>>())
                    {
                        return SetScheduledError::PromiseCompleted;
                    }
                    else if (StateIsOneOf<Faulted>())
                    {
                        return SetScheduledError::PromiseFaulted;
                    }

                    return SetScheduledError::AlreadyScheduled;
                }

                // ToDo: add OnSetScheduled() to Promise
                return DecrementCount();
            }

            SetScheduledResult DecrementCount(size_t value = 1u) noexcept
            {
                auto result = count.fetch_sub(value, std::memory_order_release);

                if (result - value != 0)
                {
                    return SetScheduledError::CannotSchedule;
                }

                state = Completed<>{};
                return SetScheduledError::PromiseCompleted;
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
                assert(this->promise != nullptr);
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

        // Returns 1 if the schedulable is already complete; otherwise 0
        template <typename TSchedulable>  // Maybe: Should be a schedulable with concept
        size_t WhenAllForEach(WhenAllPromisePtr & promise, TSchedulable & schedulable)
        {
            if constexpr (IsValueTask<TSchedulable>)
            {
                return 1u;
            }
            else
            {
                auto result = schedulable.ContinueWith(Continuation(*promise, CurrentScheduler()));

                if (!result)
                {
                    return result == AddContinuationError::PromiseCompleted
                        || result == AddContinuationError::PromiseFaulted
                        ? 1u
                        : 0u;
                }
                else
                {
                    // ToDo: check if schedulable needs scheduling
                    if constexpr (TSchedulable::CanSchedule)
                    {
                        if (schedulable.State() == TaskState::Created)
                        {
                            auto * scheduler
                                = FirstOf(schedulable.TaskScheduler(), DefaultScheduler(), CurrentScheduler());

                            assert(scheduler);

                            // Note: TrySetScheduled is called when cast to ScheduleItem
                            scheduler->Schedule(schedulable);
                        }
                    }

                    return 0u;
                }
            }
        }

    }  // namespace Detail

    // Maybe: Should be a schedulable with concept
    template <typename... TSchedulables>
    Detail::WhenAllAwaitable WhenAll(TSchedulables &&... schedulables)
    {
        auto promise = std::make_shared<Detail::WhenAllPromise>(sizeof...(TSchedulables));

        auto alreadyCompleted = (Detail::WhenAllForEach(promise, schedulables) + ...);
        promise->DecrementCount(alreadyCompleted);

        return Detail::WhenAllAwaitable(std::move(promise));
    }

}  // namespace TaskSystem