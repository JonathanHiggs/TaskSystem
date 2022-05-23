#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_0/ITaskScheduler.hpp>

#include <atomic>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>


namespace TaskSystem::inline v1_0
{

    template <typename TResult>
    class Task;

    namespace Detail
    {
        namespace States
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

            template <typename TResult>
            struct Completed final
            {
                // using rvalue_type = std::
                //     conditional_t<std::is_arithmetic_v<TResult> || std::is_pointer_v<TResult>, TResult, TResult &&>;

                TResult Value;
            };

            template <>
            struct Completed<void> final : std::monostate
            {
            };

            // Maybe: CompletedEmpty?

            struct Canceled final : std::monostate
            {
            };

            // ToDo: rename Faulted
            struct Error final
            {
                std::exception_ptr Exception;
            };

        }  // namespace States

        class TaskPromiseBase;

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

            bool await_ready() const noexcept
            {
                auto * taskScheduler = promise.TaskScheduler();
                return taskScheduler && taskScheduler->IsWorkerThread();
            }

            void await_suspend(std::coroutine_handle<>) const noexcept
            {
                auto * taskScheduler = promise.TaskScheduler();
                if (!taskScheduler)
                {
                    return;
                }

                auto handle = handle_type::from_promise(promise);

                promise.TrySchedule(*taskScheduler);
            }

            constexpr void await_resume() const noexcept
            {
                promise.TrySetRunning();
            }
        };

        class TaskFinalSuspend final
        {
        public:
            using promise_type = TaskPromiseBase;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            promise_type & promise;

        public:
            explicit TaskFinalSuspend(promise_type & promise) noexcept;

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept;

            constexpr void await_resume() const noexcept
            { }
        };

        inline constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;

        class TaskPromiseBase
        {
        protected:
            alignas(CacheLineSize) mutable std::atomic_flag resultReady;

            ITaskScheduler * taskScheduler;

            ITaskScheduler * continuationTaskScheduler;
            std::coroutine_handle<> continuation;

        public:
            TaskPromiseBase() noexcept;

            TaskFinalSuspend final_suspend() noexcept;

            std::coroutine_handle<> Continuation() const noexcept;
            void Continuation(std::coroutine_handle<> value);

            ITaskScheduler * ContinuationTaskScheduler() const noexcept;
            void ContinuationTaskScheduler(ITaskScheduler * value) noexcept;

            ITaskScheduler * TaskScheduler() const noexcept;
            void TaskScheduler(ITaskScheduler * value) noexcept;

            void Wait() const noexcept;
        };

        template <typename TResult>
        class TaskPromise final : public TaskPromiseBase
        {
        public:
            using value_type = TResult;

        private:
            using state_type = std::variant<
                States::Created,
                States::Scheduled,
                States::Running,
                States::Completed<TResult>,
                States::Canceled,
                States::Error>;

            alignas(CacheLineSize) mutable std::atomic_flag stateFlag;
            state_type state;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), state(States::Created{})
            { }

            ~TaskPromise() = default;

            Task<TResult> get_return_object() noexcept;

            TaskInitialSuspend<TaskPromise> initial_suspend() noexcept
            {
                return TaskInitialSuspend<TaskPromise>(*this);
            }

            void unhandled_exception() noexcept
            {
                while (stateFlag.test_and_set()) { }
                state = States::Error{ std::current_exception() };
                stateFlag.clear(std::memory_order_release);
            }

            template <typename TValue, typename = std::enable_if_t<std::is_convertible_v<TValue &&, TResult>>>
            void return_value(TValue && value) noexcept
            {
                while (stateFlag.test_and_set()) { }

                if constexpr (std::is_nothrow_constructible_v<TResult, TValue &&>)
                {
                    state = States::Completed<TResult>{ std::forward<TValue>(value) };
                }
                else
                {
                    try
                    {
                        state = States::Completed<TResult>{ std::forward<TValue>(value) };
                    }
                    catch (...)
                    {
                        state = States::Error{ std::current_exception() };
                    }
                }

                stateFlag.clear(std::memory_order_release);
            }

            TaskState State() const noexcept
            {
                while (stateFlag.test_and_set()) { }
                auto index = state.index();
                stateFlag.clear(std::memory_order_release);

                switch (index)
                {
                case 0u: return TaskState::Created;
                case 1u: return TaskState::Scheduled;
                case 2u: return TaskState::Running;
                case 3u: return TaskState::Completed;
                case 4u: return TaskState::Canceled;
                case 5u: return TaskState::Error;
                case 6u:
                default: return TaskState::Unknown;
                }
            }

            bool TrySchedule(ITaskScheduler & scheduler) noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!std::holds_alternative<States::Created>(state))
                {
                    stateFlag.clear(std::memory_order_release);
                    return false;
                }

                taskScheduler = &scheduler;
                state = States::Scheduled{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            bool TrySetRunning() noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!(std::holds_alternative<States::Created>(state)
                      || std::holds_alternative<States::Scheduled>(state)))
                {
                    return false;
                }

                state = States::Running{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            TResult Result() &
            {
                while (stateFlag.test_and_set()) { }

                if (auto * error = std::get_if<States::Error>(&state))
                {
                    auto ex = error->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(ex);
                }
                else if (std::holds_alternative<States::Created>(state))
                {
                    stateFlag.clear(std::memory_order_release);
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }
                // ToDo: handle other state

                auto result = std::get<States::Completed<TResult>>(state).Value;
                stateFlag.clear(std::memory_order_release);

                return result;
            }

            TResult Result() &&
            {
                while (stateFlag.test_and_set()) { }

                if (auto * error = std::get_if<States::Error>(&state))
                {
                    auto ex = error->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(ex);
                }
                else if (std::holds_alternative<States::Created>(state))
                {
                    stateFlag.clear(std::memory_order_release);
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }

                auto result = std::get<States::Completed<TResult>>(std::move(state)).Value;
                stateFlag.clear(std::memory_order_release);

                return result;
            }
        };

        template <>
        class TaskPromise<void> final : public TaskPromiseBase
        {
        public:
            using value_type = void;

        private:
            using state_type = std::variant<
                States::Created,
                States::Scheduled,
                States::Running,
                States::Completed<void>,
                States::Canceled,
                States::Error>;

            alignas(CacheLineSize) mutable std::atomic_flag stateFlag;
            state_type state;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), state(States::Created{})
            { }

            Task<void> get_return_object() noexcept;

            TaskInitialSuspend<TaskPromise> initial_suspend() noexcept
            {
                return TaskInitialSuspend<TaskPromise>(*this);
            }

            void return_void() noexcept
            {
                while (stateFlag.test_and_set()) { }

                state = States::Completed<void>();

                stateFlag.clear(std::memory_order_release);
            }

            void unhandled_exception() noexcept
            {
                while (stateFlag.test_and_set()) { }

                state = States::Error{ std::current_exception() };

                stateFlag.clear(std::memory_order_release);
            }

            TaskState State() const noexcept
            {
                while (stateFlag.test_and_set()) { }
                auto index = state.index();
                stateFlag.clear(std::memory_order_release);

                switch (index)
                {
                case 0u: return TaskState::Created;
                case 1u: return TaskState::Scheduled;
                case 2u: return TaskState::Running;
                case 3u: return TaskState::Completed;
                case 4u: return TaskState::Canceled;
                case 5u: return TaskState::Error;
                case 6u:
                default: return TaskState::Unknown;
                }
            }

            bool TrySchedule(ITaskScheduler & scheduler) noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!std::holds_alternative<States::Created>(state))
                {
                    return false;
                }

                taskScheduler = &scheduler;
                state = States::Scheduled{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            bool TrySetRunning() noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!(std::holds_alternative<States::Created>(state)
                      || std::holds_alternative<States::Scheduled>(state)))
                {
                    return false;
                }

                state = States::Running{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            void Result()
            {
                while (stateFlag.test_and_set()) { }

                if (auto * error = std::get_if<States::Error>(&state))
                {
                    auto ex = error->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Created>(state))
                {
                    stateFlag.clear(std::memory_order_release);
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }
                // ToDo: handle other states

                stateFlag.clear(std::memory_order_release);
            }
        };

        template <typename TResult>
        class TaskPromise<TResult &> final : public TaskPromiseBase
        {
        public:
            using value_type = TResult;

        private:
            // Maybe: store with unique_ptr to call deleter if needed?
            using state_type = std::variant<
                States::Created,
                States::Scheduled,
                States::Running,
                States::Completed<TResult *>,
                States::Canceled,
                States::Error>;

            alignas(CacheLineSize) mutable std::atomic_flag stateFlag;
            state_type state;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), state(States::Created{})
            { }

            Task<TResult &> get_return_object() noexcept;

            TaskInitialSuspend<TaskPromise> initial_suspend() noexcept
            {
                return TaskInitialSuspend<TaskPromise>(*this);
            }

            void return_value(TResult & value) noexcept
            {
                while (stateFlag.test_and_set()) { }

                state = States::Completed<TResult *>{ std::addressof(value) };

                stateFlag.clear(std::memory_order_release);
            }

            void unhandled_exception() noexcept
            {
                while (stateFlag.test_and_set()) { }

                state = States::Error{ std::current_exception() };

                stateFlag.clear(std::memory_order_release);
            }

            TaskState State() const noexcept
            {
                while (stateFlag.test_and_set()) { }
                auto index = state.index();
                stateFlag.clear(std::memory_order_release);

                switch (index)
                {
                case 0u: return TaskState::Created;
                case 1u: return TaskState::Scheduled;
                case 2u: return TaskState::Running;
                case 3u: return TaskState::Completed;
                case 4u: return TaskState::Canceled;
                case 5u: return TaskState::Error;
                case 6u:
                default: return TaskState::Unknown;
                }
            }

            bool TrySchedule(ITaskScheduler & scheduler) noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!std::holds_alternative<States::Created>(state))
                {
                    return false;
                }

                taskScheduler = &scheduler;
                state = States::Scheduled{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            bool TrySetRunning() noexcept
            {
                while (stateFlag.test_and_set()) { }

                if (!(std::holds_alternative<States::Created>(state)
                      || std::holds_alternative<States::Scheduled>(state)))
                {
                    return false;
                }

                state = States::Running{};

                stateFlag.clear(std::memory_order_release);

                return true;
            }

            TResult & Result()
            {
                while (stateFlag.test_and_set()) { }

                if (auto * error = std::get_if<States::Error>(&state))
                {
                    auto ex = error->Exception;
                    stateFlag.clear(std::memory_order_release);

                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Created>(state))
                {
                    stateFlag.clear(std::memory_order_release);
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }

                auto & result = *std::get<States::Completed<TResult *>>(state).Value;
                stateFlag.clear(std::memory_order_release);

                return result;
            }
        };

        template <typename TResult>
        class TaskInitialAwaiter
        {
        public:
            using promise_type = TaskPromise<TResult>;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            promise_type & promise;

        public:
            TaskInitialAwaiter(promise_type & promise) noexcept : promise(promise)
            { }

            bool await_ready() const noexcept
            {
                return false;
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> callerHandle) noexcept
            {
                promise.Continuation(callerHandle);
                return handle_type::from_promise(promise);
            }

            TResult await_resume() noexcept
            {
                return promise.return_value();
            }
        };

        template <typename TResult>
        struct TaskAwaitableBase
        {
            using promise_type = TaskPromise<TResult>;
            using handle_type = std::coroutine_handle<promise_type>;

            handle_type handle;

            TaskAwaitableBase(handle_type handle) noexcept : handle(handle)
            { }

            bool await_ready() const noexcept
            {
                return !handle || handle.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept
            {
                handle.promise().Continuation(caller);
                return handle;
            }
        };

        template <typename TResult>
        struct TaskAwaitableCopyResult final : TaskAwaitableBase<TResult>
        {
            using TaskAwaitableBase<TResult>::TaskAwaitableBase;

            TResult await_resume()
            {
                if (!this->handle)
                {
                    // ToDo: better exception
                    throw std::exception("No handle");
                }

                return this->handle.promise().Result();
            }
        };

        template <typename TResult>
        struct TaskAwaitableMoveResult final : TaskAwaitableBase<TResult>
        {
            using TaskAwaitableBase<TResult>::TaskAwaitableBase;

            TResult await_resume()
            {
                if (!this->handle)
                {
                    // ToDo: better exception
                    throw std::exception("No handle");
                }

                return std::move(this->handle.promise()).Result();
            }
        };

        struct TaskAwaitableVoid final : TaskAwaitableBase<void>
        {
            using TaskAwaitableBase::TaskAwaitableBase;

            constexpr void await_resume() const noexcept
            { }
        };

    }  // namespace Detail

    template <typename = void>
    class Task;

    template <>
    class [[nodiscard]] Task<void> final
    {
    public:
        using value_type = void;
        using promise_type = Detail::TaskPromise<value_type>;
        using handle_type = std::coroutine_handle<promise_type>;

        friend struct ITaskScheduler;

    private:
        handle_type handle;

    public:
        Task() noexcept : handle(nullptr)
        { }

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
                handle.destroy();
            }

            handle = std::exchange(other.handle, nullptr);

            return *this;
        }

        ~Task() noexcept
        {
            if (handle)
            {
                handle.destroy();
            }
        }

        inline operator std::coroutine_handle<>() const noexcept
        {
            return handle;
        }

        template <typename TFunc, typename T = std::invoke_result_t<TFunc>>
        static Task<T> From(TFunc && fn)
        {
            co_return std::forward<TFunc>(fn)();
        }

        Detail::TaskAwaitableVoid operator co_await() const noexcept
        {
            return Detail::TaskAwaitableVoid{ handle };
        }

        TaskState State() const noexcept
        {
            if (!handle)
            {
                return TaskState::Unknown;
            }

            return handle.promise().State();
        }

        bool IsReady() const noexcept
        {
            return !handle || handle.done();
        }

        void Run() &
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            handle.resume();
        }

        void Run() &&
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            handle.resume();
        }

        void Wait()
        {
            if (!handle)
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            if (handle.done())
            {
                return;
            }

            // ToDo: throw error if initialized but not scheduled?

            handle.promise().Wait();
        }

        void ScheduleOn(ITaskScheduler & taskScheduler) &
        {
            handle.promise().TrySchedule(taskScheduler);
        }

        [[nodiscard]] Task & ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            handle.promise().TaskScheduler(&taskScheduler);
            return *this;
        }
    };

    template <typename TResult>
    class [[nodiscard]] Task final
    {
    public:
        using value_type = TResult;
        using promise_type = Detail::TaskPromise<value_type>;
        using handle_type = std::coroutine_handle<promise_type>;

        friend struct ITaskScheduler;

    private:
        handle_type handle;

    public:
        Task() noexcept : handle(nullptr)
        { }

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
                handle.destroy();
            }

            handle = std::exchange(other.handle, nullptr);

            return *this;
        }

        ~Task() noexcept
        {
            if (handle)
            {
                handle.destroy();
            }
        }

        inline operator std::coroutine_handle<>() const noexcept
        {
            return handle;
        }

        template <typename TFunc, typename T = std::invoke_result_t<TFunc>>
        static Task<T> From(TFunc && fn)
        {
            co_return std::forward<TFunc>(fn)();
        }

        auto operator co_await() const & noexcept
        {
            return Detail::TaskAwaitableCopyResult<TResult>{ handle };
        }

        auto operator co_await() const && noexcept
        {
            return Detail::TaskAwaitableMoveResult<TResult>{ handle };
        }

        TaskState State() const noexcept
        {
            if (!handle)
            {
                return TaskState::Unknown;
            }

            return handle.promise().State();
        }

        bool IsReady() const noexcept
        {
            return !handle || handle.done();
        }

        template <typename TValue = TResult>
        TValue Run() &
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            if (handle.promise().State() != TaskState::Created)
            {
                throw std::exception("Task is already scheduled, cannot be run");
            }

            handle.resume();

            if constexpr (!std::is_void_v<TValue>)
            {
                return handle.promise().Result();
            }
        }

        template <typename TValue = TResult>
        TValue Run() &&
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            if (handle.promise().State() != TaskState::Created)
            {
                throw std::exception("Task is already scheduled, cannot be run");
            }

            handle.resume();

            if constexpr (!std::is_void_v<TValue>)
            {
                return std::move(handle.promise()).Result();
            }
        }

        void Wait()
        {
            if (!handle)
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            if (handle.done())
            {
                return;
            }

            // ToDo: throw error if initialized but not scheduled?

            handle.promise().Wait();
        }

        TResult Result()
        {
            Wait();
            return handle.promise().Result();
        }

        void ScheduleOn(ITaskScheduler & taskScheduler) &
        {
            handle.promise().TaskScheduler(&taskScheduler);
        }

        [[nodiscard]] Task & ScheduleOn(ITaskScheduler & taskScheduler) &&
        {
            handle.promise().TaskScheduler(&taskScheduler);
            return *this;
        }

        void ContinueOn(ITaskScheduler & taskScheduler) &
        {
            handle.promise().ContinuationTaskScheduler(taskScheduler);
        }

        [[nodiscard]] Task & ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            handle.promise().ContinuationTaskScheduler(taskScheduler);
            return *this;
        }
    };

    namespace Detail
    {

        template <typename TResult>
        Task<TResult> TaskPromise<TResult>::get_return_object() noexcept
        {
            using handle_type = std::coroutine_handle<TaskPromise>;
            return Task<TResult>(handle_type::from_promise(*this));
        }

        inline Task<void> TaskPromise<void>::get_return_object() noexcept
        {
            using handle_type = std::coroutine_handle<TaskPromise>;
            return Task<void>(handle_type::from_promise(*this));
        }

        template <typename TResult>
        Task<TResult &> TaskPromise<TResult &>::get_return_object() noexcept
        {
            using handle_type = std::coroutine_handle<TaskPromise>;
            return Task<TResult &>(handle_type::from_promise(*this));
        }

    }  // namespace Detail

}  // namespace TaskSystem::inline v1_0