#pragma once

#include <TaskSystem/ITaskScheduler.hpp>

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

            struct Initialized : std::monostate
            {
            };

            template <typename TResult>
            struct Completed
            {
                using rvalue_type = std::
                    conditional_t<std::is_arithmetic_v<TResult> || std::is_pointer_v<TResult>, TResult, TResult &&>;

                TResult Value;
            };

            template <>
            struct Completed<void> : std::monostate
            {
            };

            struct Error
            {
                std::exception_ptr Exception;
            };

        }  // namespace States

        class TaskPromiseBase;

        class TaskInitialSuspend final
        {
        public:
            using promise_type = TaskPromiseBase;
            using handle_type = std::coroutine_handle<promise_type>;

        private:
            promise_type & promise;

        public:
            explicit TaskInitialSuspend(promise_type & promise) noexcept;

            bool await_ready() const noexcept;

            void await_suspend(std::coroutine_handle<>) const noexcept;

            constexpr void await_resume() const noexcept
            { }
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

        class TaskPromiseBase
        {
        private:
            std::coroutine_handle<> continuation;
            ITaskScheduler * taskScheduler;

            // static inline constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
            // alignas(CACHE_LINE_SIZE)
            std::atomic_flag resultReady;

        public:
            TaskPromiseBase() noexcept;

            TaskInitialSuspend initial_suspend() noexcept;

            TaskFinalSuspend final_suspend() noexcept;

            std::coroutine_handle<> Continuation() const noexcept;
            void Continuation(std::coroutine_handle<> value);

            ITaskScheduler * TaskScheduler() const noexcept;
            void TaskScheduler(ITaskScheduler * value) noexcept;

            void Wait() const noexcept;
        };

        template <typename TResult>
        class TaskPromise final : public TaskPromiseBase
        {
        private:
            std::variant<States::Initialized, States::Completed<TResult>, States::Error> result;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), result(States::Initialized{})
            { }

            ~TaskPromise() = default;

            Task<TResult> get_return_object() noexcept;

            void unhandled_exception() noexcept
            {
                result = States::Error{ std::current_exception() };
            }

            template <typename TValue, typename = std::enable_if_t<std::is_convertible_v<TValue &&, TResult>>>
            void return_value(TValue && value) noexcept(std::is_nothrow_constructible_v<TResult, TValue &&>)
            {
                result = States::Completed<TResult>{ std::forward<TValue>(value) };
            }

            TResult & Result() &
            {
                if (auto * error = std::get_if<States::Error>(&result))
                {
                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Initialized>(result))
                {
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }

                return std::get<States::Completed<TResult>>(result).Value;
            }

            States::Completed<TResult>::rvalue_type Result() &&
            {
                if (auto * error = std::get_if<States::Error>(&result))
                {
                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Initialized>(result))
                {
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }

                return std::get<States::Completed<TResult>>(std::move(result)).Value;
            }
        };

        template <>
        class TaskPromise<void> final : public TaskPromiseBase
        {
        private:
            std::variant<States::Initialized, States::Completed<void>, States::Error> result;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), result(States::Initialized{})
            { }

            Task<void> get_return_object() noexcept;

            void return_void() noexcept
            {
                result = States::Completed<void>();
            }

            void unhandled_exception() noexcept
            {
                result = States::Error{ std::current_exception() };
            }

            void Result()
            {
                if (auto * error = std::get_if<States::Error>(&result))
                {
                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Initialized>(result))
                {
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }
            }
        };

        template <typename TResult>
        class TaskPromise<TResult &> final : public TaskPromiseBase
        {
        private:
            // Maybe: store with unique_ptr to call deleter if needed?
            std::variant<States::Initialized, States::Completed<TResult *>, States::Error> result;

        public:
            TaskPromise() noexcept : TaskPromiseBase(), result(States::Initialized{})
            { }

            Task<TResult &> get_return_object() noexcept;

            void return_value(TResult & value) noexcept
            {
                result = States::Completed<TResult *>{ std::addressof(value) };
            }

            void unhandled_exception() noexcept
            {
                result = States::Error{ std::current_exception() };
            }

            TResult & Result()
            {
                if (auto * error = std::get_if<States::Error>(&result))
                {
                    std::rethrow_exception(error->Exception);
                }
                else if (std::holds_alternative<States::Initialized>(result))
                {
                    // ToDo: better exception type
                    throw std::exception("Incomplete task");
                }

                return *std::get<States::Completed<TResult *>>(result).Value;
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
            handle.promise().TaskScheduler(&taskScheduler);
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

            handle.resume();

            return handle.promise().Result();
        }

        template <typename TValue = TResult>
        TValue && Run() &&
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
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

        // [[nodiscard]] Task & ContinueOn(TaskScheduler & taskScheduler) &&
        // {
        //     handle.promise().ContinuationTaskScheduler(taskScheduler);
        //     return *this;
        // }
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