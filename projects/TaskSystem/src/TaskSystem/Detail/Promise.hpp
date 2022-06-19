#pragma once

#include <TaskSystem/AtomicLockGuard.hpp>
#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/IPromise.hpp>
#include <TaskSystem/Detail/TaskStates.hpp>
#include <TaskSystem/Detail/Utils.hpp>
#include <TaskSystem/ITaskScheduler.hpp>
#include <TaskSystem/TaskState.hpp>

#include <atomic>
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
        { T::ScheduleContinuations } -> std::convertible_to<bool>;
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

        [[nodiscard]] bool TryAddContinuation(Detail::Continuation value) noexcept override final
        {
            if (!value)
            {
                return false;
            }

            std::lock_guard lock(stateFlag);

            if (!StateIsOneOf<Created, Scheduled, Running, Suspended>())
            {
                return false;
            }

            continuations.Add(std::move(value));

            return true;
        }

        [[nodiscard]] ITaskScheduler * ContinuationScheduler() const noexcept override final
        {
            return continuationScheduler;
        }

        void ContinuationScheduler(ITaskScheduler * value) noexcept override final { continuationScheduler = value; }

        [[nodiscard]] bool TrySetScheduled() noexcept override
        {
            // Maybe: two separate versions, one is constexpr
            if constexpr (!policy_type::CanSchedule)
            {
                return false;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    return false;
                }

                state = Scheduled{};
                return true;
            }
        }

        [[nodiscard]] bool TrySetRunning() noexcept override final
        {
            if constexpr (!policy_type::CanRun)
            {
                return false;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Scheduled, Suspended>())
                {
                    return false;
                }

                state = Running{};
                return true;
            }
        }

        [[nodiscard]] bool TrySetRunning(IgnoreAlreadySetTag) noexcept override final
        {
            if constexpr (!policy_type::CanRun)
            {
                return false;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if (StateIsOneOf<Running>())
                {
                    return true;
                }

                if (!StateIsOneOf<Created, Scheduled, Suspended>())
                {
                    return false;
                }

                state = Running{};
                return true;
            }
        }

        [[nodiscard]] bool TrySetSuspended() noexcept override final
        {
            if constexpr (!policy_type::CanSuspend)
            {
                return false;
            }
            else
            {
                std::lock_guard lock(stateFlag);

                if constexpr (policy_type::AllowSuspendFromCreated)
                {
                    if (!StateIsOneOf<Created, Running>())
                    {
                        return false;
                    }
                }
                else
                {
                    if (!StateIsOneOf<Running>())
                    {
                        return false;
                    }
                }

                state = Suspended{};
                return true;
            }
        }

        [[nodiscard]] bool TrySetException(std::exception_ptr ex) noexcept override final
        {
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Running, Suspended>())
                {
                    return false;
                }

                state = Faulted{ ex };
                this->ScheduleContinuations();
            }

            completeFlag.test_and_set(std::memory_order_acquire);
            completeFlag.notify_all();

            return true;
        }

        void Wait() const noexcept override final
        {
            // Waits for TrySetResult, TrySetCompleted or TrySetException to set completeFlag to true
            completeFlag.wait(false, std::memory_order_acquire);
        }

    protected:
        inline void ScheduleContinuations() noexcept override final
        {
            if constexpr (policy_type::ScheduleContinuations)
            {
                for (auto & continuation : this->continuations)
                {
                    auto * scheduler = FirstOf(
                        continuation.Scheduler(),
                        this->continuationScheduler,
                        DefaultScheduler(),
                        CurrentScheduler());

                    // ToDo: Assert scheduler

                    if (!continuation.Promise().TrySetScheduled())
                    {
                        if (continuation.Promise().State().IsCompleted())
                        {
                            continuation.Promise().ScheduleContinuations();
                        }

                        continue;
                    }

                    scheduler->Schedule(continuation.Promise());
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

        [[nodiscard]] bool TrySetResult(std::convertible_to<TResult> auto && value) noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    return false;
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

            return true;
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

        [[nodiscard]] bool TrySetResult(TResult & value) noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    return false;
                }

                this->state = Completed<TResult *>{ std::addressof(value) };
                this->ScheduleContinuations();
            }

            this->completeFlag.test_and_set(std::memory_order_acquire);
            this->completeFlag.notify_all();

            return true;
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

        [[nodiscard]] bool TrySetCompleted() noexcept
        {
            {
                std::lock_guard lock(this->stateFlag);

                if (!this->StateIsOneOf<Created, Running, Suspended>())
                {
                    return false;
                }

                this->state = Completed<>{};
                this->ScheduleContinuations();
            }

            this->completeFlag.test_and_set(std::memory_order_acquire);
            this->completeFlag.notify_all();

            return true;
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