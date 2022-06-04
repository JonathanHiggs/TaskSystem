#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_1/Detail/Continuation.hpp>
#include <TaskSystem/v1_1/Detail/TaskStates.hpp>
#include <TaskSystem/v1_1/Detail/Utils.hpp>
#include <TaskSystem/v1_1/AtomicLockGuard.hpp>
#include <TaskSystem/v1_1/ITaskScheduler.hpp>
#include <TaskSystem/v1_1/ScheduleItem.hpp>

#include <atomic>
#include <cassert>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>


namespace TaskSystem::v1_1
{

    template <typename TResult, typename TPromise>
    class Task;

    namespace Detail
    {

        template <typename TPromise>
        class TaskInitialSuspend final
        {
        public:
            using value_type = typename TPromise::value_type;
            using promise_type = TPromise;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            promise_type & promise;

        public:
            explicit TaskInitialSuspend(promise_type & promise) noexcept : promise(promise)
            { }

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            void await_suspend(std::coroutine_handle<>) const noexcept
            {
                auto * taskScheduler = promise.TaskScheduler();
                if (!taskScheduler)
                {
                    return;
                }

                // Check this thread set the promise to scheduled, i.e. no other thread beat us
                auto set = promise.TrySetScheduled();
                if (set)
                {
                    auto handle = handle_type::from_promise(promise);
                    taskScheduler->Schedule(ScheduleItem(std::coroutine_handle<>(handle)));
                }
            }

            void await_resume() const noexcept
            {
#if _DEBUG
                // Check this thread set the promise to running
                auto set = promise.TrySetRunning();
                assert(set);
#else
                promise.TrySetRunning();
#endif
            }
        };

        template <typename TPromise>
        class TaskFinalSuspend final
        {
        public:
            using value_type = typename TPromise::value_type;
            using promise_type = TPromise;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            promise_type & promise;

        public:
            explicit TaskFinalSuspend(promise_type & promise) noexcept : promise(promise)
            { }

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept
            {
                auto continuation = promise.Continuation();
                if (!continuation)
                {
                    return std::noop_coroutine();
                }

                // Maybe: possible optimization - avoid schedule and rethrow
                // if (promise.State() == TaskState::Error)
                // {
                //     continuation.SetError(promise.ExceptionPtr());
                // }

                auto * scheduler =
                    Detail::FirstOf(continuation.Scheduler(), promise.ContinuationScheduler(), DefaultScheduler());

                if (!scheduler || IsCurrentScheduler(scheduler))
                {
                    // Check: continuation state is set to running?
                    return continuation.Handle();
                }

                // Schedule continuation to run on different scheduler
                // Check: continuation state is set to scheduled?
                scheduler->Schedule(ScheduleItem(continuation.Handle()));

                return std::noop_coroutine();
            }

            constexpr void await_resume() const noexcept
            { }
        };

        // Maybe: template on the exception type rather than assuming exception_ptr?
        template <typename TResult>
        class TaskPromise final
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromise;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            using state_type = std::variant<Created, Scheduled, Running, Suspended, Completed<TResult>, Faulted>;

#pragma warning(disable : 4324)
            // Disable: warning C4324: structure was padded due to alignment specifier
            // alignment pads out the promise but also ensures the two atomic_flags are on different cache lines

            // Note: can use std::atomic_flag after ABI break
            alignas(CacheLineSize) mutable std::atomic<bool> stateFlag;
            alignas(CacheLineSize) mutable std::atomic_flag completeFlag;
#pragma warning(default : 4234)

            // Note: assumes the scheduler's lifetime will exceed the coroutine execution
            ITaskScheduler * taskScheduler = nullptr;
            ITaskScheduler * continuationScheduler = nullptr;

            // Maybe: might need more than the handle to set continuation scheduled or running
            //        Handle, ExecutingScheduler, SetRunning, SetSuspended
            Detail::Continuation continuation = nullptr;

            state_type state = Created{};

            template <typename... Ts>
            bool StateIsOneOf()
            {
                return (std::holds_alternative<Ts>(state) || ...);
            }

        public:
            Task<TResult, TaskPromise> get_return_object() noexcept
            {
                return Task<TResult, TaskPromise>(handle_type::from_promise(*this));
            }

            TaskInitialSuspend<promise_type> initial_suspend() noexcept
            {
                return TaskInitialSuspend<promise_type>(*this);
            }

            TaskFinalSuspend<promise_type> final_suspend() noexcept
            {
                return TaskFinalSuspend<promise_type>(*this);
            }

            void unhandled_exception() noexcept
            {
                {
                    std::lock_guard lock(stateFlag);
                    state = Faulted{ std::current_exception() };
                }

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();
            }

            template <typename TValue, typename = std::enable_if_t<std::is_convertible_v<TValue &&, TResult>>>
            void return_value(TValue && value) noexcept
            {
                {
                    std::lock_guard lock(stateFlag);

                    if constexpr (std::is_nothrow_constructible_v<TResult, TValue&&>)
                    {
                        state = Completed<TResult>{ std::forward<TValue>(value) };
                    }
                    else
                    {
                        try
                        {
                            state = Completed<TResult>{ std::forward<TValue>(value) };
                        }
                        catch (...)
                        {
                            state = Faulted{ std::current_exception() };
                        }
                    }
                }

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();
            }

            [[nodiscard]] TaskState State() const noexcept
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

            // Returns true if the method was able to atomically set the state to Scheduled; false otherwise
            [[nodiscard]] bool TrySetScheduled() noexcept
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Suspended>())
                {
                    return false;
                }

                state = Scheduled{};

                return true;
            }

            // Returns true if the method was able to atomically set the state to Running; false otherwise
            [[nodiscard]] bool TrySetRunning() noexcept
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Scheduled, Suspended>())
                {
                    return false;
                }

                state = Running{};

                return true;
            }

            // Returns true if the method was able to atomically set the state to Suspended; false otherwise
            [[nodiscard]] bool TrySetSuspended() noexcept
            {
                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Running>())
                {
                    return false;
                }

                state = Suspended{};

                return true;
            }

            [[nodiscard]] Detail::Continuation const & Continuation() const noexcept
            {
                return continuation;
            }

            // Maybe: TryAddContinuation?
            [[nodiscard]] bool TrySetContinuation(Detail::Continuation value)
            {
                if (!value)
                {
                    // Maybe: return error code ContinuationAlreadySet
                    return false;
                }

                std::lock_guard lock(stateFlag);

                if (!StateIsOneOf<Created, Scheduled, Running, Suspended>())
                {
                    return false;
                }

                continuation = std::move(value);

                return true;
            }

            [[nodiscard]] ITaskScheduler * TaskScheduler() const noexcept
            {
                return taskScheduler;
            }

            void TaskScheduler(ITaskScheduler * value) noexcept
            {
                taskScheduler = value;
            }

            [[nodiscard]] ITaskScheduler * ContinuationScheduler() const noexcept
            {
                return continuationScheduler;
            }

            void ContinuationScheduler(ITaskScheduler * value) noexcept
            {
                continuationScheduler = value;
            }

            [[nodiscard]] TResult Result() &
            {
                std::lock_guard lock(stateFlag);

                if (StateIsOneOf<Created, Scheduled, Running, Suspended>())
                {
                    throw std::exception("Task is not complete");
                }
                else if (auto * fault = std::get_if<Faulted>(&state))
                {
                    std::rethrow_exception(fault->Exception);
                }

                return std::get<Completed<TResult>>(state).Value;
            }

            [[nodiscard]] TResult Result() &&
            {
                std::lock_guard lock(stateFlag);

                if (StateIsOneOf<Created, Scheduled, Running, Suspended>())
                {
                    throw std::exception("Task is not complete");
                }
                else if (auto * fault = std::get_if<Faulted>(&state))
                {
                    std::rethrow_exception(fault->Exception);
                }

                return std::get<Completed<TResult>>(std::move(state)).Value;
            }

            void Wait() const noexcept
            {
                // Waits for return_value or unhandled_exception to set completeFlag to true
                completeFlag.wait(false, std::memory_order_acquire);
            }
        };

        // ToDo: template <typename TResult, bool MoveResult>
        template <typename TResult, bool MoveResult>
        class TaskAwaitable final
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromise<TResult>;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            handle_type handle;

        public:
            TaskAwaitable(handle_type handle) noexcept : handle(handle)
            { }

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            template <typename TPromise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> caller) noexcept
            {
                if (!handle || handle.done() || handle.promise().State().IsCompleted())
                {
                    return caller;
                }

                if (!caller.promise().TrySetSuspended())
                {
                    // ToDo: what to do here?
                    assert(false);
                }

                if (!handle.promise().TrySetContinuation(Detail::Continuation(caller)))
                {
                    // throw std::exception("Unable to set continuation");
                    assert(false);
                }

                // Maybe: template on the caller promise type, can read the current scheduler out of the promise?
                auto * scheduler = handle.promise().TaskScheduler();
                if (scheduler && !IsCurrentScheduler(scheduler))
                {
                    if (handle.promise().TrySetScheduled())
                    {
                        scheduler->Schedule(ScheduleItem(std::coroutine_handle<>(handle)));
                    }
                    return std::noop_coroutine();
                }

                if (!handle.promise().TrySetScheduled())
                {
                    // ToDo: should never happen
                    return std::noop_coroutine();
                }

                return handle;
            }

            TResult await_resume()
            {
                if (!handle)
                {
                    throw std::exception("Cannot resume null handle");
                }

                if constexpr (MoveResult)
                {
                    return std::move(handle.promise()).Result();
                }
                else
                {
                    return handle.promise().Result();
                }
            }
        };

    }  // namespace Detail

    template <typename TResult, typename TPromise = Detail::TaskPromise<TResult>>
    class [[nodiscard]] Task final
    {
    public:
        using value_type = TResult;
        using promise_type = TPromise;
        using handle_type = std::coroutine_handle<promise_type>;

    private:
        handle_type handle;

    public:
        explicit Task(handle_type handle) noexcept : handle(handle)
        { }

        Task(Task const &) = delete;
        Task & operator=(Task const &) = delete;

        Task(Task && other) noexcept : handle(std::exchange(other.handle, nullptr))
        { }

        Task & operator=(Task && other) noexcept
        {
            if (std::addressof(other) == this)
            {
                return *this;
            }

            if (handle)
            {
                if (handle.promise().State() == TaskState::Running)
                {
                    // ToDo: either throw and loose the noexcept, or wait for completion and maybe deadlock
                    //       maybe add orphaned flag to promise so it can clean up when transferring to continuation?
                }

                handle.destroy();
            }

            handle = std::exchange(other.handle, nullptr);

            return *this;
        }

        ~Task() noexcept
        {
            if (handle)
            {
                if (handle.promise().State() == TaskState::Running)
                {
                    // ToDo: either throw and loose the noexcept, or wait for completion and maybe deadlock
                    //       maybe add orphaned flag to promise so it can clean up when transferring to continuation?
                }

                handle.destroy();
            }
        }

        [[nodiscard]] operator ScheduleItem() const
        {
            if (!handle)
            {
                throw std::exception("Invalid handle");
            }

            if (!handle.promise().TrySetScheduled())
            {
                throw std::exception("Task is already scheduled");
            }

            return ScheduleItem(std::coroutine_handle<>(handle));
        }

        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        [[nodiscard]] static Task<TResult> From(TFunc && func)
        {
            co_return std::forward<TFunc>(func)();
        }

        auto operator co_await() const & noexcept
        {
            return Detail::TaskAwaitable<TResult, false>(handle);
        }

        auto operator co_await() const && noexcept
        {
            return Detail::TaskAwaitable<TResult, true>(handle);
        }

        [[nodiscard]] TaskState State() const noexcept
        {
            if (!handle)
            {
                // Maybe: TaskState::Destroyed?
                return TaskState::Unknown;
            }

            return handle.promise().State();
        }

        void Wait() const noexcept
        {
            if (!handle)
            {
                return;
            }

            handle.promise().Wait();
        }

        [[nodiscard]] TResult Result()
        {
            // ToDo: lvalue / rvalue versions
            Wait();
            return handle.promise().Result();
        }

        Task & ScheduleOn(ITaskScheduler & taskScheduler) &
        {
            handle.promise().TaskScheduler(&taskScheduler);
            return *this;
        }

        [[nodiscard]] Task && ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            handle.promise().TaskScheduler(&taskScheduler);
            return std::move(*this);
        }

        Task & ContinueOn(ITaskScheduler & taskScheduler) &
        {
            handle.promise().ContinuationScheduler(&taskScheduler);
            return *this;
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            handle.promise().ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    // ToDo: Task<void> specialization

    // ToDo: Task<TResult &> specialization

    // Maybe: Task::FromResult() task with no coroutine/promise
    // template <typename TResult>
    // Task<TResult, void>

}  // namespace TaskSystem::v1_1