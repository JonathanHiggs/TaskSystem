#pragma once

#include <TaskSystem/AtomicLockGuard.hpp>
#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/IPromise.hpp>
#include <TaskSystem/Detail/SetCompletedResult.hpp>
#include <TaskSystem/Detail/TaskStates.hpp>
#include <TaskSystem/Detail/Utils.hpp>
#include <TaskSystem/ITaskScheduler.hpp>
#include <TaskSystem/TaskState.hpp>

#include <atomic>
#include <cassert>
#include <concepts>
#include <exception>
#include <mutex>
#include <type_traits>
#include <variant>


namespace TaskSystem::Detail
{

    template <typename T>
    concept PromisePolicy = requires
    {
        // clang-format off

        // ToDo: find out why std::same_as<bool> does not work
        { T::CanSchedule } -> std::convertible_to<bool>;
        { T::CanRun } -> std::convertible_to<bool>;
        { T::CanSuspend } -> std::convertible_to<bool>;
        { T::AllowSuspendFromCreated } -> std::convertible_to<bool>;
        // Maybe: AllowSetRunningWhenRunning

        // clang-format on
    };

    // Maybe:
    // struct PromisePolicyDefaults
    // {
    //     static inline constexpr bool AllowSuspendFromCreated = false;
    // };

    template <typename TResult, PromisePolicy TPolicy>
    class PromiseBase : public IPromise
    {
    protected:
        using policy_type = TPolicy;
        using state_type = std::variant<Created, Scheduled, Running, Suspended, Completed<TResult>, Faulted>;

#pragma warning(disable : 4324)
        // Disable: warning C4324: structure was padded due to alignment specifier
        // alignment pads out the promise but also ensures the two atomic_flags are on different cache lines

        // Note: can use std::atomic_flag after ABI break
        alignas(CacheLineSize) mutable std::atomic<bool> stateFlag;
        alignas(CacheLineSize) mutable std::atomic_flag completeFlag;
#pragma warning(default : 4324)

        state_type state = Created{};

        Detail::Continuations continuations{};
        ITaskScheduler * continuationScheduler = nullptr;

        template <typename... Ts>
        bool StateIsOneOf()
        {
            return (std::holds_alternative<Ts>(state) || ...);
        }

    public:
        // ToDo: handle delete promise while thread is waiting
        ~PromiseBase() noexcept override = default;

        [[nodiscard]] TaskState State() const noexcept override final
        {
            size_t index;
            {
                std::lock_guard lock(stateFlag);
                index = state.index();
            }

            switch (index)
            {
            case 0u: return TaskState::Created;
            case 1u: return TaskState::Scheduled;
            case 2u: return TaskState::Running;
            case 3u: return TaskState::Suspended;
            case 4u: return TaskState::Completed;
            case 5u: return TaskState::Error;
            default: return TaskState::Unknown;
            }
        }

        [[nodiscard]] Detail::Continuations & Continuations() noexcept override final { return continuations; }

        [[nodiscard]] AddContinuationResult TryAddContinuation(Detail::Continuation value) noexcept override final
        {
            if (!value)
            {
                return AddContinuationError::InvalidContinuation;
            }

            std::lock_guard lock(stateFlag);

            if (!StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                if (StateIsOneOf<Completed<TResult>>())
                {
                    return AddContinuationError::PromiseCompleted;
                }
                else if (StateIsOneOf<Faulted>())
                {
                    return AddContinuationError::PromiseFaulted;
                }

                std::terminate();
            }

            continuations.Add(std::move(value));

            return Success;
        }

        [[nodiscard]] ITaskScheduler * ContinuationScheduler() const noexcept override final
        {
            return continuationScheduler;
        }

        void ContinuationScheduler(ITaskScheduler * value) noexcept override final { continuationScheduler = value; }

        [[nodiscard]] SetScheduledResult TrySetScheduled() noexcept override
        {
            // Maybe: two separate versions, one is constexpr
            if constexpr (!policy_type::CanSchedule)
            {
                return SetScheduledError::CannotSchedule;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    if (StateIsOneOf<Running>())
                    {
                        return SetScheduledError::PromiseRunning;
                    }
                    else if (StateIsOneOf<Completed<TResult>>())
                    {
                        return SetScheduledError::PromiseCompleted;
                    }
                    else if (StateIsOneOf<Faulted>())
                    {
                        return SetScheduledError::PromiseFaulted;
                    }

                    return SetScheduledError::AlreadyScheduled;
                }

                state = Scheduled{};
                return Success;
            }
        }

        [[nodiscard]] SetRunningResult TrySetRunning() noexcept override final
        {
            if constexpr (!policy_type::CanRun)
            {
                return SetRunningError::CannotRun;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Scheduled, Suspended>())
                {
                    if (StateIsOneOf<Completed<TResult>>())
                    {
                        return SetRunningError::PromiseCompleted;
                    }
                    if (StateIsOneOf<Faulted>())
                    {
                        return SetRunningError::PromiseFaulted;
                    }

                    return SetRunningError::AlreadyRunning;
                }

                state = Running{};
                return Success;
            }
        }

        [[nodiscard]] SetSuspendedResult TrySetSuspended() noexcept override final
        {
            if constexpr (!policy_type::CanSuspend)
            {
                return SetSuspendedError::CannotSuspend;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if constexpr (policy_type::AllowSuspendFromCreated)
                {
                    if (!StateIsOneOf<Created, Running>())
                    {
                        if (StateIsOneOf<Scheduled>())
                        {
                            return SetSuspendedError::PromiseScheduled;
                        }
                        if (StateIsOneOf<Completed<TResult>>())
                        {
                            return SetSuspendedError::PromiseCompleted;
                        }
                        if (StateIsOneOf<Faulted>())
                        {
                            return SetSuspendedError::PromiseFaulted;
                        }

                        return SetSuspendedError::AlreadySuspended;
                    }
                }
                else
                {
                    if (!StateIsOneOf<Running>())
                    {
                        if (StateIsOneOf<Created>())
                        {
                            return SetSuspendedError::PromiseCreated;
                        }
                        if (StateIsOneOf<Scheduled>())
                        {
                            return SetSuspendedError::PromiseScheduled;
                        }
                        if (StateIsOneOf<Completed<TResult>>())
                        {
                            return SetSuspendedError::PromiseCompleted;
                        }
                        if (StateIsOneOf<Faulted>())
                        {
                            return SetSuspendedError::PromiseFaulted;
                        }

                        return SetSuspendedError::AlreadySuspended;
                    }
                }

                state = Suspended{};
                return Success;
            }
        }

        [[nodiscard]] SetFaultedResult TrySetException(std::exception_ptr ex) noexcept override final
        {
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Running, Suspended>())
                {
                    if (StateIsOneOf<Scheduled>())
                    {
                        return SetFaultedError::PromiseScheduled;
                    }
                    if (StateIsOneOf<Completed<TResult>>())
                    {
                        return SetFaultedError::PromiseCompleted;
                    }

                    return SetFaultedError::AlreadyFaulted;
                }

                state = Faulted{ ex };
                this->ScheduleContinuations();
            }

            completeFlag.test_and_set(std::memory_order_acquire);
            completeFlag.notify_all();

            return Success;
        }

        void Wait() const noexcept override final
        {
            // Waits for TrySetResult, TrySetCompleted or TrySetException to set completeFlag to true
            completeFlag.wait(false, std::memory_order_acquire);
        }

        void ScheduleContinuations() noexcept override final
        {
            for (auto & continuation : this->continuations)
            {
                auto * scheduler = FirstOf(
                    continuation.Scheduler(),
                    this->continuationScheduler,
                    DefaultScheduler(),
                    CurrentScheduler());

                assert(scheduler);

                auto result = continuation.Promise().TrySetScheduled();
                if (result)
                {
                    scheduler->Schedule(continuation.Promise());
                }
                else
                {
                    if (result == SetScheduledError::PromiseCompleted || result == SetScheduledError::PromiseFaulted)
                    {
                        continuation.Promise().ScheduleContinuations();
                    }
                }
            }
        }
    };

    template <typename TResult, PromisePolicy TPolicy>
    class Promise : public PromiseBase<TResult, TPolicy>
    {
    public:
        ~Promise() noexcept override = default;

        [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

        [[nodiscard]] SetCompletedResult TrySetResult(std::convertible_to<TResult> auto && value) noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    if (this->StateIsOneOf<Scheduled>())
                    {
                        return SetCompletedError::PromiseScheduled;
                    }
                    if (this->StateIsOneOf<Faulted>())
                    {
                        return SetCompletedError::PromiseFaulted;
                    }

                    return SetCompletedError::AlreadyCompleted;
                }

                if constexpr (std::is_nothrow_constructible_v<TResult, decltype(value)>)
                {
                    this->state = Completed<TResult>{ std::forward<decltype(value)>(value) };
                }
                else
                {
                    try
                    {
                        this->state = Completed<TResult>{ std::forward<decltype(value)>(value) };
                    }
                    catch (...)
                    {
                        this->state = Faulted{ std::current_exception() };
                    }
                }

                this->ScheduleContinuations();
            }

            this->completeFlag.test_and_set(std::memory_order_acquire);
            this->completeFlag.notify_all();

            return Success;
        }

        [[nodiscard]] TResult & Result() &
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return std::get<Completed<TResult>>(this->state).Value;
        }

        [[nodiscard]] TResult const & Result() const &
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return std::get<Completed<TResult>>(this->state).Value;
        }

        [[nodiscard]] TResult Result() &&
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return std::get<Completed<TResult>>(std::move(this->state)).Value;
        }

        [[nodiscard]] TResult const && Result() const &&
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return std::get<Completed<TResult>>(std::move(this->state)).Value;
        }
    };

    template <typename TResult, PromisePolicy TPolicy>
    class Promise<TResult &, TPolicy> : public PromiseBase<TResult *, TPolicy>
    {
    public:
        ~Promise() noexcept override = default;

        [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

        [[nodiscard]] SetCompletedResult TrySetResult(TResult & value) noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    if (this->StateIsOneOf<Scheduled>())
                    {
                        return SetCompletedError::PromiseScheduled;
                    }
                    if (this->StateIsOneOf<Faulted>())
                    {
                        return SetCompletedError::PromiseFaulted;
                    }

                    return SetCompletedError::AlreadyCompleted;
                }

                this->state = Completed<TResult *>{ std::addressof(value) };
                this->ScheduleContinuations();
            }

            this->completeFlag.test_and_set(std::memory_order_acquire);
            this->completeFlag.notify_all();

            return Success;
        }

        [[nodiscard]] TResult & Result()
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return *std::get<Completed<TResult *>>(this->state).Value;
        }

        [[nodiscard]] TResult const & Result() const
        {
            std::lock_guard lock(this->stateFlag);

            if (this->StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                throw std::exception("Task is not complete");
            }
            else if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }

            return *std::get<Completed<TResult *>>(this->state).Value;
        }
    };

    template <PromisePolicy TPolicy>
    class Promise<void, TPolicy> : public PromiseBase<void, TPolicy>
    {
    public:
        ~Promise() noexcept override = default;

        [[nodiscard]] std::coroutine_handle<> Handle() noexcept override { return std::noop_coroutine(); }

        [[nodiscard]] SetCompletedResult TrySetCompleted() noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    if (this->StateIsOneOf<Scheduled>())
                    {
                        return SetCompletedError::PromiseScheduled;
                    }
                    if (this->StateIsOneOf<Faulted>())
                    {
                        return SetCompletedError::PromiseFaulted;
                    }

                    return SetCompletedError::AlreadyCompleted;
                }

                this->state = Completed<>{};
                this->ScheduleContinuations();
            }

            this->completeFlag.test_and_set(std::memory_order_acquire);
            this->completeFlag.notify_all();

            return Success;
        }

        [[nodiscard]] void ThrowIfFaulted() const
        {
            std::lock_guard lock(this->stateFlag);

            if (auto * fault = std::get_if<Faulted>(&this->state))
            {
                std::rethrow_exception(fault->Exception);
            }
        }
    };

}  // namespace TaskSystem::Detail