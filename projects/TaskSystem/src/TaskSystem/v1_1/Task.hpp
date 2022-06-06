#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_1/AtomicLockGuard.hpp>
#include <TaskSystem/v1_1/Detail/Continuation.hpp>
#include <TaskSystem/v1_1/Detail/Promise.hpp>
#include <TaskSystem/v1_1/Detail/TaskStates.hpp>
#include <TaskSystem/v1_1/Detail/Utils.hpp>
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
                auto * taskScheduler = FirstOf(promise.TaskScheduler(), DefaultScheduler());
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
                if (!promise.TrySetRunning())
                {
                    throw std::exception("Unable to set task running");
                }
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

        struct TaskPromisePolicy final
        {
            static inline constexpr bool ScheduleContinuations = false;
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
        };

        // Maybe: template on the exception type rather than assuming exception_ptr?
        template <typename TResult>
        class TaskPromise final : public Promise<TaskPromisePolicy, TResult>
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromise;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            ITaskScheduler * taskScheduler = nullptr;

        public:
            ~TaskPromise() noexcept override = default;

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
                this->TrySetException(std::current_exception());
            }

            void return_value(std::convertible_to<TResult> auto && value) noexcept
            {
                this->TrySetResult(std::forward<decltype(value)>(value));
            }

            [[nodiscard]] ITaskScheduler * TaskScheduler() const noexcept
            {
                return taskScheduler;
            }

            void TaskScheduler(ITaskScheduler * value) noexcept
            {
                taskScheduler = value;
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