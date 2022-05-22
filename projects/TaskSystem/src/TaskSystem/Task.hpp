#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>


namespace TaskSystem
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
            TaskInitialSuspend(promise_type & promise) noexcept : promise(promise)
            { }

            constexpr auto await_ready() const noexcept
            {
                return false;
                // ToDo:
                // auto * taskManager = promise.TaskManager();
                // return taskManager && taskManager->IsWorkerThread();
            }

            void await_suspend(std::coroutine_handle<>) const noexcept
            {
                // ToDo:
                // auto * taskManager = promise.TaskManager();
                // if (!taskManager) {
                //     return;
                // }
                //
                // auto handle = handle_type::from_promise(promise);
                // taskManager->Schedule(handle);
            }

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
            explicit TaskFinalSuspend(promise_type & promise) noexcept : promise(promise)
            { }

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
            // static inline constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

            std::coroutine_handle<> continuation;
            // TaskManager * taskManager;

            // alignas(CACHE_LINE_SIZE)
            std::atomic_flag resultReady;

        public:
            TaskPromiseBase() noexcept : continuation(nullptr)
            {
                resultReady.clear(std::memory_order::relaxed);
            }

            TaskInitialSuspend initial_suspend() noexcept
            {
                return TaskInitialSuspend(*this);
            }

            TaskFinalSuspend final_suspend() noexcept
            {
                resultReady.test_and_set();  // Maybe: can this be memory_order_relaxed?
                resultReady.notify_all();

                return TaskFinalSuspend(*this);
            }

            std::coroutine_handle<> Continuation() const noexcept
            {
                return continuation;
            }

            void Continuation(std::coroutine_handle<> value)
            {
                if (continuation != nullptr)
                {
                    // throw std::exception("Multiple continuations for a single promise");
                }

                continuation = value;
            }

            void Wait() const noexcept
            {
                resultReady.wait(false);
            }

            // TaskManager * TaskManager() const noexcept
            // {
            //     return taskManager;
            // }
            //
            // void TaskManager(TaskManager * value) noexcept
            // {
            //     taskManager = value;
            // }
        };

        std::coroutine_handle<> TaskFinalSuspend::await_suspend(std::coroutine_handle<>) const noexcept
        {
            auto continuation = promise.Continuation();
            return continuation ? continuation : std::noop_coroutine();
        }

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

    }  // namespace Detail

    template <typename TResult = void>
    class [[nodiscard]] Task final
    {
    public:
        using value_type = TResult;
        using promise_type = Detail::TaskPromise<value_type>;
        using handle_type = std::coroutine_handle<promise_type>;

    private:
        struct AwaitableBase
        {
            using handle_type = std::coroutine_handle<promise_type>;
            using void_handle_type = std::coroutine_handle<>;

            handle_type handle;

            AwaitableBase(handle_type handle) noexcept : handle(handle)
            { }

            bool await_ready() const noexcept
            {
                return !handle || handle.done();
            }

            void_handle_type await_suspend(void_handle_type caller) noexcept
            {
                handle.promise().Continuation(caller);
                return handle;
            }
        };

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

        template <typename TFunc, typename T = std::invoke_result_t<TFunc>>
        static Task<T> From(TFunc && fn)
        {
            co_return std::forward<TFunc>(fn)();
        }

        auto operator co_await() const & noexcept
        {
            struct Awaitable final : AwaitableBase
            {
                using AwaitableBase::AwaitableBase;

                decltype(auto) await_resume()
                {
                    if (!this->handle)
                    {
                        // ToDo: better exception
                        throw std::exception("No handle");
                    }

                    return this->handle.promise().Result();
                }
            };

            return Awaitable{ handle };
        }

        auto operator co_await() const && noexcept
        {
            struct Awaitable final : AwaitableBase
            {
                using AwaitableBase::AwaitableBase;

                decltype(auto) await_resume()
                {
                    if (!this->handle)
                    {
                        // ToDo: better exception
                        throw std::exception("No handle");
                    }

                    return std::move(this->handle.promise()).Result();
                }
            };

            return Awaitable{ handle };
        }

        bool IsReady() const noexcept
        {
            return !handle || handle.done();
        }

        template <typename TValue = TResult, std::enable_if_t<!std::is_void_v<TValue>> * = nullptr>
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

        template <typename TValue = TResult, std::enable_if_t<std::is_void_v<TValue>> * = nullptr>
        void Run() &
        {
            if (!handle || handle.done())
            {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            handle.resume();
        }

        template <typename TValue = TResult, std::enable_if_t<!std::is_void_v<TValue>> * = nullptr>
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

        template <typename TValue = TResult, std::enable_if_t<std::is_void_v<TValue>> * = nullptr>
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

        [[nodiscard]] TResult Result()
        {
            Wait();
            return handle.promise().Result();
        }

        // [[nodiscard]] Task & ScheduleOn(TaskManager & taskManager) &&
        // {
        //     handle.promise().TaskManager(taskManager);
        //     return *this;
        // }
        //
        // [[nodiscard]] Task & ContinueOn(TaskManager & taskManager) &&
        // {
        //     handle.promise().ContinuationTaskManager(taskManager);
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

}  // namespace TaskSystem