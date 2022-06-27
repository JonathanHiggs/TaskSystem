#pragma once

#include <TaskSystem/Detail/Promise.hpp>
#include <TaskSystem/ValueTask.hpp>

#include <atomic>
#include <cassert>
#include <coroutine>
#include <memory>
#include <vector>


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
            std::vector<IPromise *> continuationOf;

        public:
            WhenAnyPromise(size_t count) noexcept : resumed(false) { continuationOf.reserve(count); }

            [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

            void AddContinuationOf(IPromise & promise) noexcept { continuationOf.emplace_back(&promise); }

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

                return CheckResume();
            }

            SetScheduledResult CheckResume() noexcept
            {
                auto result = resumed.exchange(true, std::memory_order_acquire);

                if (result != false)
                {
                    return SetScheduledError::CannotSchedule;
                }

                state = Completed<>{};

                for (auto* promise : continuationOf)
                {
                    if (promise == nullptr)
                    {
                        continue;
                    }

                    //promise->TryRemoveContinuation(this);
                }

                return SetScheduledError::PromiseCompleted;
            }
        };

        using WhenAnyPromisePtr = std::shared_ptr<WhenAnyPromise>;

        class WhenAnyAwaitable
        {
        private:
            WhenAnyPromisePtr promise;

        public:
            WhenAnyAwaitable(WhenAnyPromisePtr promise) noexcept : promise(std::move(promise))
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
        size_t WhenAnyForEach(WhenAnyPromisePtr & promise, TSchedulable & schedulable)
        {
            // Maybe: this is similar to WhenAllForEach... might combine
            if constexpr (IsValueTask<TSchedulable>)
            {
                return 1u;
            }
            else
            {
                auto result = schedulable.ContinueWith(Continuation(*promise, CurrentScheduler()));
                //promise->AddContinuationOf(schedulable.Promise());

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

    template <typename... TSchedulables>
    Detail::WhenAnyAwaitable WhenAny(TSchedulables &&... schedulables)
    {
        auto promise = std::make_shared<Detail::WhenAnyPromise>();

        auto alreadyCompleted = (Detail::WhenAnyForEach(promise, schedulables) + ...);
        if (alreadyCompleted > 0u)
        {
            promise->CheckResume();
        }

        return Detail::WhenAnyAwaitable(std::move(promise));
    }

}  // namespace TaskSystem