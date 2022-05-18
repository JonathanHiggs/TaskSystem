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

        class TaskFinalAwaitable
        {
        public:
            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            template <typename TResult>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TResult> coroutine) const noexcept
            {
                auto continuation = coroutine.promise().Continuation();
                return continuation ? continuation : std::noop_coroutine();
            }

            constexpr void await_resume() const noexcept
            { }
        };

        class TaskPromiseBase
        {
        private:
            std::coroutine_handle<> continuation;

        public:
            TaskPromiseBase() noexcept : continuation(nullptr)
            { }

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            TaskFinalAwaitable final_suspend() noexcept
            {
                return {};
            }

            void SetContinuation(std::coroutine_handle<> value) noexcept
            {
                continuation = value;
            }

            std::coroutine_handle<> Continuation() noexcept
            {
                return continuation;
            }
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
                // ToDo: should move into the result?
                result = States::Completed<TResult>{ value };
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

    }  // namespace Detail

    template <typename TResult = void>
    class [[nodiscard]] Task
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
                handle.promise().SetContinuation(caller);
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

        auto operator co_await() const & noexcept
        {
            struct Awaitable : AwaitableBase
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
            struct Awaitable : AwaitableBase
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

        // Maybe: might not want to keep this
        TResult Run()
        {
            if (!handle || handle.done()) {
                // ToDo: better exception
                throw std::exception("Nope");
            }

            handle.resume();

            return handle.promise().Result();
        }
    };

    namespace Detail
    {

        template<typename TResult>
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

    }

}  // namespace TaskSystem