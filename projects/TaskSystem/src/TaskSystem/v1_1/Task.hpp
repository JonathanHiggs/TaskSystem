#pragma once

#include <TaskSystem/TaskState.hpp>
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

    template <typename TResult>
    class Task;

    namespace Detail
    {

        struct Created final : std::monostate
        {
        };

        struct Scheduled final : std::monostate
        {
        };

        struct Running final : std::monostate
        {
        };

        // ToDo: add Suspended

        template <typename TResult>
        struct Completed final
        {
            TResult Value;
        };

        template <>
        struct Completed<void> final : std::monostate
        {
        };

        // ToDo: add CompletedResultMoved

        struct Faulted final
        {
            std::exception_ptr Exception;
        };

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

                auto * continuationScheduler = promise.ContinuationScheduler();
                if (!continuationScheduler || IsCurrentScheduler(continuationScheduler))
                {
                    // Check: continuation state is set to running?
                    return continuation;
                }

                // Schedule continuation to run on different scheduler
                // Check: continuation state is set to scheduled?
                continuationScheduler->Schedule(ScheduleItem(continuation));

                return std::noop_coroutine();
            }

            constexpr void await_resume() const noexcept
            { }
        };

        inline constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;

        template <typename TResult>
        class TaskPromise final
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromise;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            using state_type = std::variant<Created, Scheduled, Running, Completed<TResult>, Faulted>;

            alignas(CacheLineSize) mutable std::atomic_flag completeFlag;

            // Note: assumes the scheduler's lifetime will exceed the coroutine execution
            ITaskScheduler * taskScheduler = nullptr;
            ITaskScheduler * continuationScheduler = nullptr;

            // Maybe: might need more than the handle to set continuation scheduled or running
            //        Handle, ExecutingScheduler, SetRunning, SetSuspended
            std::coroutine_handle<> continuation = nullptr;

            // Note: stateFlag is a spin lock to synchronise access to state, most likely there won't be many
            //       threads contending for access so it should be nice and fast
#pragma warning(disable : 4324)
            // Disable: warning C4324: structure was padded due to alignment specifier
            // alignment pads out the promise but also ensures the two atomic_flags are on different cache lines
            alignas(CacheLineSize) mutable std::atomic_flag stateFlag;
#pragma warning(default : 4234)
            state_type state = Created{};

            template <typename... Ts>
            bool StateIsOneOf()
            {
                return (std::holds_alternative<Ts>(state) || ...);
            }

        public:
            Task<TResult> get_return_object() noexcept
            {
                return Task<TResult>(handle_type::from_promise(*this));
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
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }
                state = Faulted{ std::current_exception() };
                stateFlag.clear(std::memory_order_release);

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();
            }

            template <typename TValue, typename = std::enable_if_t<std::is_convertible_v<TValue &&, TResult>>>
            void return_value(TValue && value) noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if constexpr (std::is_nothrow_constructible_v<TResult, TValue &&>)
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

                stateFlag.clear(std::memory_order_release);

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();
            }

            TaskState State() const noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }
                auto index = state.index();
                stateFlag.clear(std::memory_order_release);

                switch (index)
                {
                case 0u: return TaskState::Created;
                case 1u: return TaskState::Scheduled;
                case 2u: return TaskState::Running;
                case 3u: return TaskState::Completed;
                case 4u: return TaskState::Error;
                default: return TaskState::Unknown;
                }
            }

            // Returns true if the method was able to atomically set the state to Scheduled; false otherwise
            bool TrySetScheduled() noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (!StateIsOneOf<Created>())
                {
                    stateFlag.clear(std::memory_order_release);
                    return false;
                }

                state = Scheduled{};
                stateFlag.clear(std::memory_order_release);

                return true;
            }

            // Returns true if the method was able to atomically set the state to Running; false otherwise
            bool TrySetRunning() noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (!StateIsOneOf<Created, Scheduled>())
                {
                    stateFlag.clear(std::memory_order_release);
                    return false;
                }

                state = Running{};
                stateFlag.clear(std::memory_order_release);

                return true;
            }

            std::coroutine_handle<> Continuation() const noexcept
            {
                return continuation;
            }

            void Continuation(std::coroutine_handle<> value)
            {
                continuation = value;
            }

            ITaskScheduler * ContinuationScheduler() const noexcept
            {
                return continuationScheduler;
            }

            void ContinuationScheduler(ITaskScheduler * value) noexcept
            {
                continuationScheduler = value;
            }

            ITaskScheduler * TaskScheduler() const noexcept
            {
                return taskScheduler;
            }

            void TaskScheduler(ITaskScheduler * value) noexcept
            {
                taskScheduler = value;
            }

            [[nodiscard]] TResult Result() &
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (StateIsOneOf<Created, Scheduled, Running>())
                {
                    stateFlag.clear(std::memory_order_release);
                    throw std::exception("Task is not complete");
                }
                else if (auto * fault = std::get_if<Faulted>(&state))
                {
                    auto ex = fault->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(ex);
                }

                auto result = std::get<Completed<TResult>>(state).Value;
                stateFlag.clear(std::memory_order_release);

                return result;
            }

            [[nodiscard]] TResult Result() &&
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (StateIsOneOf<Created, Scheduled, Running>())
                {
                    stateFlag.clear(std::memory_order_release);
                    throw std::exception("Task is not complete");
                }
                else if (auto * fault = std::get_if<Faulted>(&state))
                {
                    auto ex = fault->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(ex);
                }

                auto result = std::get<Completed<TResult>>(std::move(state)).Value;
                stateFlag.clear(std::memory_order_release);

                return result;
            }

            // ToDo: TryResult

            void Wait() const noexcept
            {
                // Waits for return_value to set completeFlag true
                completeFlag.wait(false, std::memory_order_acquire);
            }
        };

        // ToDo: template <typename TResult, bool MoveResult>
        template <typename TResult>
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

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept
            {
                if (!handle || handle.done() || handle.promise().State().IsCompleted())
                {
                    return caller;
                }

                // ToDo: caller.promise().SetSuspended();

                handle.promise().Continuation(caller);

                // Maybe: template on the caller promise type, can read the current scheduler out of the promise?
                auto * taskScheduler = handle.promise().TaskScheduler();
                if (taskScheduler && !IsCurrentScheduler(taskScheduler))
                {
                    if (handle.promise().TrySetScheduled())
                    {
                        taskScheduler->Schedule(ScheduleItem(std::coroutine_handle<>(handle)));
                    }
                    return std::noop_coroutine();
                }

                if (!handle.promise().TrySetRunning())
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

                // ToDo: move result from promise if template MoveResult
                return handle.promise().Result();
            }
        };

    }  // namespace Detail

    // Maybe: template on Promise and Await?
    template <typename TResult>
    class [[nodiscard]] Task final
    {
    public:
        using value_type = TResult;
        using promise_type = Detail::TaskPromise<value_type>;
        using handle_type = std::coroutine_handle<promise_type>;

    private:
        handle_type handle;

        friend class promise_type;

        explicit Task(handle_type handle) noexcept : handle(handle)
        { }

    public:
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

        Detail::TaskAwaitable<TResult> operator co_await() const noexcept
        {
            // ToDo: lvalue / rvalue versions
            return Detail::TaskAwaitable<TResult>(handle);
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

        TResult Result()
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

}  // namespace TaskSystem::v1_1