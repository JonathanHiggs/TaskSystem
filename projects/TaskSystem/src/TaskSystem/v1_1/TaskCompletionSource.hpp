#pragma once

#include <TaskSystem/v1_1/AtomicLockGuard.hpp>
#include <TaskSystem/v1_1/Awaitable.hpp>
#include <TaskSystem/v1_1/Detail/Continuation.hpp>
#include <TaskSystem/v1_1/Detail/Promise.hpp>
#include <TaskSystem/v1_1/Detail/TaskStates.hpp>
#include <TaskSystem/v1_1/Detail/Utils.hpp>
#include <TaskSystem/v1_1/ITask.hpp>
#include <TaskSystem/v1_1/Task.hpp>

#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>
#include <variant>


namespace TaskSystem::v1_1
{
    namespace Detail
    {

        struct TaskCompletionSourcePromisePolicy final
        {
            static inline constexpr bool ScheduleContinuations = true;
            static inline constexpr bool CanSchedule = false;
            static inline constexpr bool CanRun = false;
            static inline constexpr bool CanSuspend = false;
        };

        template <typename TResult>
        using TaskCompletionSourcePromise = Promise<TResult, TaskCompletionSourcePromisePolicy>;

        template <typename TResult, bool MoveResult>
        class TaskCompletionSourceAwaitable final
        {
        public:
            using value_type = TResult;
            using promise_type = TaskCompletionSourcePromise<TResult>;

        private:
            promise_type & promise;

        public:
            TaskCompletionSourceAwaitable(promise_type & promise) noexcept : promise(promise) { }

            constexpr bool await_ready() const noexcept { return false; }

            template <PromiseType TPromise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> callerHandle)
            {
                IPromise & callerPromise = callerHandle.promise();
                return await_suspend(callerHandle, callerPromise);
            }

            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<> callerHandle, IPromise & callerPromise)
            {
                if (promise.State().IsCompleted())
                {
                    return callerHandle;
                }

                if (!callerPromise.TrySetSuspended())
                {
                    // ToDo: what to do here?
                    throw std::exception("Unable to set caller promise to suspended");
                }

                // Suspend the caller and don't schedule anything new
                if (!promise.TrySetContinuation(Detail::Continuation(&callerPromise, callerHandle, CurrentScheduler())))
                {
                    // Maybe: check is status is completed and return caller handle;
                    throw std::exception("Unable to schedule continuation");
                }

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
    class [[nodiscard]] Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> final : public ITask<TResult>
    {
    public:
        using value_type = TResult;
        using promise_type = Detail::TaskCompletionSourcePromise<TResult>;

    private:
        promise_type & promise;

    public:
        ~Task() noexcept override = default;

        explicit Task(promise_type & promise) noexcept : promise(promise) { }

        auto operator co_await() const & noexcept
        {
            return Detail::TaskCompletionSourceAwaitable<TResult, false>(promise);
        }

        auto operator co_await() const && noexcept
        {
            return Detail::TaskCompletionSourceAwaitable<TResult, true>(promise);
        }

        [[nodiscard]] TaskState State() const noexcept override { return promise.State(); }

        void Wait() const noexcept override { return promise.Wait(); }

        [[nodiscard]] TResult & Result() & override
        {
            Wait();
            return promise.Result();
        }

        [[nodiscard]] TResult const & Result() const & override
        {
            Wait();
            return promise.Result();
        }

        [[nodiscard]] TResult && Result() && override
        {
            Wait();
            return std::move(promise.Result());
        }

        [[nodiscard]] TResult const && Result() const && override
        {
            Wait();
            return std::move(promise.Result());
        }

    protected:
        [[nodiscard]] Awaitable<TResult> GetAwaitable() const & noexcept override
        {
            return Awaitable<TResult>(Detail::TaskCompletionSourceAwaitable<TResult, false>(promise));
        }

        [[nodiscard]] Awaitable<TResult> GetAwaitable() const && noexcept override
        {
            return Awaitable<TResult>(Detail::TaskCompletionSourceAwaitable<TResult, true>(promise));
        }
    };

    template <typename TResult>
    class TaskCompletionSource
    {
    private:
        Detail::TaskCompletionSourcePromise<TResult> promise;

    public:
        TaskCompletionSource() noexcept = default;

        TaskCompletionSource(TaskCompletionSource const &) = delete;
        TaskCompletionSource & operator=(TaskCompletionSource const &) = delete;

        TaskCompletionSource(TaskCompletionSource &&) = delete;
        TaskCompletionSource & operator=(TaskCompletionSource &&) = delete;

        [[nodiscard]] ::TaskSystem::v1_1::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> Task()
        {
            return ::TaskSystem::v1_1::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>>(promise);
        }

        template <typename TValue, std::enable_if_t<std::is_convertible_v<TValue &&, TResult>> * = nullptr>
        [[nodiscard]] bool TrySetResult(TValue && value) noexcept(
            std::is_nothrow_constructible_v<TResult, decltype(value)>)
        {
            return promise.TrySetResult(std::forward<TValue>(value));
        }

        template <typename TValue, std::enable_if_t<std::is_convertible_v<TValue &&, TResult>> * = nullptr>
        void SetResult(TValue && value)
        {
            auto result = TrySetResult(std::forward<TValue>(value));
            if (!result)
            {
                throw std::exception("Unable to set value");
            }
        }

        template <typename TException, std::enable_if_t<!std::is_same_v<TException, std::exception_ptr>> * = nullptr>
        [[nodiscard]] bool TrySetException(TException && exception) noexcept
        {
            return promise.TrySetException(std::make_exception_ptr(std::forward<TException>(exception)));
        }

        template <typename TException, std::enable_if_t<std::is_same_v<TException, std::exception_ptr>> * = nullptr>
        [[nodiscard]] bool TrySetException(std::exception_ptr exception) noexcept
        {
            return promise.TrySetException(exception);
        }

        template <typename TException>
        void SetException(TException && exception)
        {
            auto result = TrySetException(std::forward<TException>(exception));
            if (!result)
            {
                throw std::exception("Unable to set exception");
            }
        }
    };

}  // namespace TaskSystem::v1_1