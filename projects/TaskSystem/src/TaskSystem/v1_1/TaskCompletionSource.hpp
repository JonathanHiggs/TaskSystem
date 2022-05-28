#pragma once

#include <TaskSystem/v1_1/Task.hpp>

#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>


namespace TaskSystem::v1_1
{
    namespace Detail
    {

        template <typename TResult>
        class TaskCompletionSourcePromise final
        {
        private:
            using state_type = std::variant<Created, Completed<TResult>, Faulted>;

#pragma warning(disable : 4324)
            // Disable: warning C4324: structure was padded due to alignment specifier
            // alignment pads out the promise but also ensures the two atomic_flags are on different cache lines

            alignas(CacheLineSize) mutable std::atomic_flag stateFlag;
            alignas(CacheLineSize) mutable std::atomic_flag completeFlag;
#pragma warning(default : 4324)

            state_type state = Created{};
            ITaskScheduler * continuationScheduler = nullptr;
            std::coroutine_handle<> continuation = nullptr;

            template <typename... Ts>
            bool StateIsOneOf()
            {
                return (std::holds_alternative<Ts>(state) || ...);
            }

        public:
            [[nodiscard]] TaskState State() const noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }
                auto index = state.index();
                stateFlag.clear(std::memory_order_release);

                switch (index)
                {
                case 0u: return TaskState::Created;
                case 1u: return TaskState::Completed;
                case 2u: return TaskState::Error;
                default: return TaskState::Unknown;
                }
            }

            [[nodiscard]] std::coroutine_handle<> Continuation() const noexcept
            {
                return continuation;
            }

            void Continuation(std::coroutine_handle<> value)
            {
                continuation = value;
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
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (StateIsOneOf<Created>())
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

                if (StateIsOneOf<Created>())
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

            template <typename TValue, typename = std::enable_if_t<std::is_convertible_v<TValue &&, TResult>>>
            [[nodiscard]] bool TrySetResult(TValue && value) noexcept
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (!StateIsOneOf<Created>())
                {
                    stateFlag.clear(std::memory_order_release);
                    return false;
                }

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

                continuationScheduler->Schedule(continuation);

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();

                return true;
            }

            [[nodiscard]] bool TrySetException(std::exception_ptr ex)
            {
                while (stateFlag.test_and_set(std::memory_order_acquire)) { }

                if (!StateIsOneOf<Created>())
                {
                    stateFlag.clear(std::memory_order_release);
                    return false;
                }

                state = Faulted{ ex };

                stateFlag.clear(std::memory_order_release);

                continuationScheduler->Schedule(continuation);

                // Release any threads waiting for the result
                completeFlag.test_and_set(std::memory_order_acquire);
                completeFlag.notify_all();

                return true;
            }

            void Wait() const noexcept
            {
                // Waits for TryGetResult or TrySetException to set complete flag to true
                completeFlag.wait(false, std::memory_order_acquire);
            }
        };

        template <typename TResult, bool MoveResult>
        class TaskCompletionSourceAwaitable
        {
        public:
            using value_type = TResult;
            using promise_type = TaskCompletionSourcePromise<TResult>;

        private:
            promise_type & promise;

        public:
            TaskCompletionSourceAwaitable(promise_type & promise) noexcept : promise(promise)
            { }

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            template <typename TPromise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> caller)
            {
                if (promise.State().IsCompleted())
                {
                    return caller;
                }

                if (!caller.promise().TrySetSuspended())
                {
                    // ToDo: what to do here?
                    assert(false);
                }

                // Suspend the caller and don't schedule anything new
                promise.Continuation(caller);
                promise.ContinuationScheduler(CurrentScheduler());

                return std::noop_coroutine();
            }

            TResult await_resume()
            {
                if constexpr (MoveResult)
                {
                    return std::move(promise).Result();
                }
                else
                {
                    return promise.Result();
                }
            }
        };

    }  // namespace Detail

    template <typename TResult>
    class [[nodiscard]] Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> final
    {
    public:
        using value_type = TResult;
        using promise_type = Detail::TaskCompletionSourcePromise<TResult>;

    private:
        promise_type & promise;

    public:
        explicit Task(promise_type & promise) noexcept : promise(promise)
        { }

        auto operator co_await() const& noexcept
        {
            return Detail::TaskCompletionSourceAwaitable<TResult, false>(promise);
        }

        auto operator co_await() const && noexcept
        {
            return Detail::TaskCompletionSourceAwaitable<TResult, true>(promise);
        }

        [[nodiscard]] TaskState State() const noexcept
        {
            return promise.State();
        }

        void Wait() const noexcept
        {
            return promise.Wait();
        }

        [[nodiscard]] TResult Result()
        {
            Wait();
            return promise.Result();
        }
    };

    template <typename TResult>
    class TaskCompletionSource
    {
    private:
        Detail::TaskCompletionSourcePromise<TResult> promise;

    public:
        [[nodiscard]] ::TaskSystem::v1_1::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> Task()
        {
            return ::TaskSystem::v1_1::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>>(promise);
        }

        template <typename TValue, std::enable_if_t<std::is_convertible_v<TValue &&, TResult>> * = nullptr>
        bool TrySetResult(TValue && value)
        {
            return promise.TrySetResult(std::forward<TValue>(value));
        }

        template <typename TException>
        bool TrySetException(TException && exception)
        {
            return promise.TrySetException(std::forward<TException>(exception));
        }
    };

}  // namespace TaskSystem::v1_1