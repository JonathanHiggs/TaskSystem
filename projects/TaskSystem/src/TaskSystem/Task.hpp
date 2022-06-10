#pragma once

#include <TaskSystem/AtomicLockGuard.hpp>
#include <TaskSystem/Awaitable.hpp>
#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/Promise.hpp>
#include <TaskSystem/Detail/TaskStates.hpp>
#include <TaskSystem/Detail/Utils.hpp>
#include <TaskSystem/ITask.hpp>
#include <TaskSystem/ITaskScheduler.hpp>
#include <TaskSystem/ScheduleItem.hpp>
#include <TaskSystem/TaskState.hpp>

#include <atomic>
#include <cassert>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>


namespace TaskSystem
{

    namespace Detail
    {
        template <typename TResult>
        class TaskPromise;
    }

    template <typename TResult, typename TPromise = Detail::TaskPromise<TResult>>
    class Task;

    namespace Detail
    {

#pragma region Awaitables

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
            explicit TaskInitialSuspend(promise_type & promise) noexcept : promise(promise) { }

            constexpr bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<>) const noexcept
            {
                auto * taskScheduler = promise.TaskScheduler();
                if (!taskScheduler)
                {
                    return;
                }

                // Check this thread set the promise to scheduled, i.e. no other thread beat us
                if (promise.TrySetScheduled())
                {
                    auto handle = handle_type::from_promise(promise);
                    taskScheduler->Schedule(ScheduleItem(std::coroutine_handle<>(handle)));
                }
            }

            void await_resume() const
            {
                // Check this thread set the promise to running
                if (!promise.TrySetRunning())
                {
                    throw std::exception("Unable to set task running");
                }
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
            explicit TaskFinalSuspend(promise_type & promise) noexcept : promise(promise) { }

            constexpr bool await_ready() const noexcept { return false; }

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

                auto * scheduler
                    = Detail::FirstOf(continuation.Scheduler(), promise.ContinuationScheduler(), DefaultScheduler());

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

            constexpr void await_resume() const noexcept { }
        };

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
            TaskAwaitable(handle_type handle) noexcept : handle(handle) { }

            constexpr bool await_ready() const noexcept { return false; }

            template <typename TPromise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> callerHandle)
            {
                IPromise & callerPromise = callerHandle.promise();
                return await_suspend(callerHandle, callerPromise);
            }

            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<> callerHandle, IPromise & callerPromise)
            {
                if (!handle || handle.done() || handle.promise().State().IsCompleted())
                {
                    return callerHandle;
                }

                if (!callerPromise.TrySetSuspended())
                {
                    throw std::exception("Unable to set caller promise to suspended");
                }

                if (!handle.promise().TrySetContinuation(
                        Detail::Continuation(&callerPromise, callerHandle)))  // ToDo: current scheduler?
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

                if constexpr (!std::same_as<TResult, void>)
                {
                    if constexpr (MoveResult)
                    {
                        return std::move(handle.promise()).Result();
                    }
                    else
                    {
                        return handle.promise().Result();
                    }
                }
                else
                {
                    // Maybe?
                    // handle.promise().ThrowIfFaulted();
                }
            }
        };

#pragma endregion

#pragma region TaskPromise

        struct TaskPromisePolicy final
        {
            static inline constexpr bool ScheduleContinuations = false;
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
        };

        template <typename TResult, typename TImpl>
        class TaskPromiseBase : public Promise<TResult, TaskPromisePolicy>
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromiseBase;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            ITaskScheduler * taskScheduler = nullptr;

        public:
            ~TaskPromiseBase() noexcept override = default;

            TaskInitialSuspend<promise_type> initial_suspend() noexcept
            {
                return TaskInitialSuspend<promise_type>(*this);
            }

            TaskFinalSuspend<promise_type> final_suspend() noexcept { return TaskFinalSuspend<promise_type>(*this); }

            void unhandled_exception() noexcept { this->TrySetException(std::current_exception()); }

            [[nodiscard]] ITaskScheduler * TaskScheduler() const noexcept override { return taskScheduler; }

            void TaskScheduler(ITaskScheduler * value) noexcept override { taskScheduler = value; }
        };

        template <typename TResult>
        class TaskPromise final : public TaskPromiseBase<TResult, TaskPromise<TResult>>
        {
        public:
            using promise_type = TaskPromise<TResult>;
            using handle_type = std::coroutine_handle<promise_type>;
            using task_type = Task<TResult, promise_type>;

            task_type get_return_object() noexcept { return task_type(handle_type::from_promise(*this)); }

            void return_value(std::convertible_to<TResult> auto && value) noexcept
            {
                this->TrySetResult(std::forward<decltype(value)>(value));
            }
        };

        template <typename TResult>
        class TaskPromise<TResult &> : public TaskPromiseBase<TResult &, TaskPromise<TResult &>>
        {
        public:
            using promise_type = TaskPromise<TResult &>;
            using handle_type = std::coroutine_handle<promise_type>;
            using task_type = Task<TResult &, TaskPromise<TResult &>>;

            task_type get_return_object() noexcept { return task_type(handle_type::from_promise(*this)); }

            void return_value(TResult & value) noexcept { this->TrySetResult(value); }
        };

        template <>
        class TaskPromise<void> final : public TaskPromiseBase<void, TaskPromise<void>>
        {
        public:
            using promise_type = TaskPromise;
            using handle_type = std::coroutine_handle<promise_type>;

            Task<void, TaskPromise<void>> get_return_object() noexcept;

            void return_void() noexcept
            {
                if (!this->TrySetCompleted())
                {
                }
            }
        };

#pragma endregion

#pragma region Task

        template <typename TResult>
        class TaskBase : public ITask<TResult>
        {
        public:
            using value_type = TResult;
            using promise_type = TaskPromise<TResult>;
            using handle_type = std::coroutine_handle<promise_type>;

        protected:
            handle_type handle;

        public:
            explicit TaskBase(handle_type handle) noexcept : handle(handle) { }

            TaskBase(TaskBase const &) = delete;
            TaskBase & operator=(TaskBase const &) = delete;

            TaskBase(TaskBase && other) noexcept : handle(std::exchange(other.handle, nullptr)) { }

            TaskBase & operator=(TaskBase && other) noexcept
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
                        //       maybe add orphaned flag to promise so it can clean up when transferring to
                        //       continuation?
                    }

                    handle.destroy();
                }

                handle = std::exchange(other.handle, nullptr);

                return *this;
            }

            ~TaskBase() noexcept override
            {
                if (handle)
                {
                    if (handle.promise().State() == TaskState::Running)
                    {
                        // ToDo: either throw and loose the noexcept, or wait for completion and maybe deadlock
                        //       maybe add orphaned flag to promise so it can clean up when transferring to
                        //       continuation?
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

                // ToDo: maybe add promise to ScheduleItem and call TrySetScheduled elsewhere
                if (!handle.promise().TrySetScheduled())
                {
                    throw std::exception("Unable to schedule task");
                }

                return ScheduleItem(std::coroutine_handle<>(handle));
            }

            auto operator co_await() const & noexcept { return TaskAwaitable<TResult, false>(handle); }
            auto operator co_await() const && noexcept { return TaskAwaitable<TResult, true>(handle); }

            [[nodiscard]] TaskState State() const noexcept override final
            {
                if (!handle)
                {
                    return TaskState::Unknown;
                }

                return handle.promise().State();
            }

            void Wait() const noexcept override final
            {
                if (!handle)
                {
                    return;
                }

                handle.promise().Wait();
            }

            void ScheduleOn(ITaskScheduler & taskScheduler) &
            {
                handle.promise().TaskScheduler(&taskScheduler);
                return *this;
            }

            void ContinueOn(ITaskScheduler & taskScheduler) &
            {
                handle.promise().ContinuationScheduler(&taskScheduler);
                return *this;
            }

        protected:
            [[nodiscard]] Awaitable<TResult> GetAwaitable() const & noexcept override
            {
                return Awaitable<TResult>(TaskAwaitable<TResult, false>(handle));
            }

            [[nodiscard]] Awaitable<TResult> GetAwaitable() const && noexcept override
            {
                return Awaitable<TResult>(TaskAwaitable<TResult, true>(handle));
            }
        };

    }  // namespace Detail

    template <typename TResult>
    class [[nodiscard]] Task<TResult, Detail::TaskPromise<TResult>> final : public Detail::TaskBase<TResult>
    {
    public:
        using base_type = Detail::TaskBase<TResult>;

        using value_type = TResult;
        using promise_type = Detail::TaskPromise<TResult>;
        using handle_type = std::coroutine_handle<promise_type>;

    public:
        explicit Task(handle_type handle) noexcept : base_type(handle) { }

        Task(Task const &) = delete;
        Task & operator=(Task const &) = delete;

        Task(Task && other) noexcept : base_type(std::move(other)) { }

        Task & operator=(Task && other) noexcept
        {
            base_type::operator=(std::move(other));
            return *this;
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        [[nodiscard]] static Task<TResult, Detail::TaskPromise<TResult>> From(TFunc && func)
        {
            co_return std::forward<TFunc>(func)();
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func)
        {
            Run(std::forward<TFunc>(func), DefaultScheduler());
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func, ITaskScheduler & scheduler)
        {
            scheduler.Schedule(ScheduleItem(From(std::forward<TFunc>(func))));
        }

        [[nodiscard]] TResult & Result() & override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return this->handle.promise().Result();
        }

        [[nodiscard]] TResult const & Result() const & override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return this->handle.promise().Result();
        }

        [[nodiscard]] TResult && Result() && override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return std::move(this->handle.promise().Result());
        }

        [[nodiscard]] TResult const && Result() const && override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return std::move(this->handle.promise().Result());
        }

        [[nodiscard]] Task && ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().TaskScheduler(&taskScheduler);
            return std::move(*this);
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    template <typename TResult>
    class [[nodiscard]] Task<TResult &, Detail::TaskPromise<TResult &>> final : public Detail::TaskBase<TResult &>
    {
    public:
        using base_type = Detail::TaskBase<TResult &>;

        using value_type = TResult &;
        using promise_type = Detail::TaskPromise<TResult &>;
        using handle_type = std::coroutine_handle<promise_type>;

    public:
        explicit Task(handle_type handle) noexcept : base_type(handle) { }

        Task(Task const &) = delete;
        Task & operator=(Task const &) = delete;

        Task(Task && other) noexcept : base_type(std::move(other)) { }

        Task & operator=(Task && other) noexcept
        {
            base_type::operator=(std::move(other));
            return *this;
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult &, std::invoke_result_t<TFunc>>> * = nullptr>
        [[nodiscard]] static Task<TResult &, Detail::TaskPromise<TResult &>> From(TFunc && func)
        {
            co_return std::forward<TFunc>(func)();
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func)
        {
            Run(std::forward<TFunc>(func), DefaultScheduler());
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_same_v<TResult, std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func, ITaskScheduler & scheduler)
        {
            scheduler.Schedule(ScheduleItem(From(std::forward<TFunc>(func))));
        }

        [[nodiscard]] TResult & Result() override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return this->handle.promise().Result();
        }

        [[nodiscard]] TResult const & Result() const override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            return this->handle.promise().Result();
        }

        [[nodiscard]] Task && ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().TaskScheduler(&taskScheduler);
            return std::move(*this);
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    template <>
    class [[nodiscard]] Task<void, Detail::TaskPromise<void>> final : public Detail::TaskBase<void>
    {
    public:
        using base_type = Detail::TaskBase<void>;

        using value_type = void;
        using promise_type = Detail::TaskPromise<void>;
        using handle_type = std::coroutine_handle<promise_type>;

    public:
        explicit Task(handle_type handle) noexcept : base_type(handle) { }

        Task(Task const &) = delete;
        Task & operator=(Task const &) = delete;

        Task(Task<void, Detail::TaskPromise<void>> && other) noexcept : base_type(std::move(other)) { }

        Task & operator=(Task && other) noexcept
        {
            base_type::operator=(std::move(other));
            return *this;
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_void_v<std::invoke_result_t<TFunc>>> * = nullptr>
        [[nodiscard]] static Task<void, Detail::TaskPromise<void>> From(TFunc && func)
        {
            std::forward<TFunc>(func)();
            co_return;
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_void_v<std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func)
        {
            Run(std::forward<TFunc>(func), DefaultScheduler());
        }

        // ToDo: use concept
        template <typename TFunc, std::enable_if_t<std::is_void_v<std::invoke_result_t<TFunc>>> * = nullptr>
        static void Run(TFunc && func, ITaskScheduler & scheduler)
        {
            scheduler.Schedule(ScheduleItem(From(std::forward<TFunc>(func))));
        }

        void ThrowIfFaulted() const override
        {
            if (!this->handle)
            {
                throw std::exception("Invalid handle");
            }

            this->Wait();
            this->handle.promise().ThrowIfFaulted();
        }

        [[nodiscard]] Task && ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().TaskScheduler(&taskScheduler);
            return std::move(*this);
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->handle.promise().ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    // Maybe: Task::FromResult() task with no coroutine/promise
    // template <typename TResult>
    // Task<TResult, void>

#pragma endregion

    namespace Detail
    {

        inline Task<void, TaskPromise<void>> TaskPromise<void>::get_return_object() noexcept
        {
            return Task<void, TaskPromise<void>>(handle_type::from_promise(*this));
        }

    }  // namespace Detail

}  // namespace TaskSystem