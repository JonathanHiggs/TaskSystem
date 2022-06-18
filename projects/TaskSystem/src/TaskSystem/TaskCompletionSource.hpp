#pragma once

#include <TaskSystem/AtomicLockGuard.hpp>
#include <TaskSystem/Awaitable.hpp>
#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/Promise.hpp>
#include <TaskSystem/Detail/TaskStates.hpp>
#include <TaskSystem/Detail/Utils.hpp>
#include <TaskSystem/ITask.hpp>
#include <TaskSystem/Task.hpp>

#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>
#include <variant>


namespace TaskSystem
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

        // ToDo: TaskCompletionSourcePromise should std::enable_shared_from_this
        template <typename TResult>
        class TaskCompletionSourcePromise final : public Promise<TResult, TaskCompletionSourcePromisePolicy>
        {
        };

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
                if (!promise.TryAddContinuation(Detail::Continuation(callerPromise, CurrentScheduler())))
                {
                    // Maybe: check is status is completed and return caller handle;
                    throw std::exception("Unable to schedule continuation");
                }

                return std::noop_coroutine();
            }

            TResult await_resume()
            {
                if constexpr (!std::same_as<TResult, void>)
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
            }
        };

#pragma region Task

        template <typename TResult>
        class CompletionTaskBase : public ITask<TResult>
        {
        public:
            using value_type = TResult;
            using promise_type = Detail::TaskCompletionSourcePromise<TResult>;
            using handle_type = void;

        protected:
            // ToDo: std::shared_ptr<promise_type>
            promise_type & promise;

        public:
            ~CompletionTaskBase() noexcept override = default;

            explicit CompletionTaskBase(promise_type & promise) noexcept : promise(promise) { }

            auto operator co_await() const & noexcept { return TaskCompletionSourceAwaitable<TResult, false>(promise); }
            auto operator co_await() const && noexcept { return TaskCompletionSourceAwaitable<TResult, true>(promise); }

            [[nodiscard]] TaskState State() const noexcept override { return promise.State(); }

            void Wait() const noexcept override { return promise.Wait(); }

            void ScheduleOn(ITaskScheduler & taskScheduler) & { }

            void ContinueOn(ITaskScheduler & taskScheduler) & { promise.ContinuationScheduler(&taskScheduler); }

        protected:
            [[nodiscard]] Awaitable<TResult> GetAwaitable() & noexcept override
            {
                return Awaitable<TResult>(Detail::TaskCompletionSourceAwaitable<TResult, false>(promise));
            }

            [[nodiscard]] Awaitable<TResult> GetAwaitable() && noexcept override
            {
                return Awaitable<TResult>(Detail::TaskCompletionSourceAwaitable<TResult, true>(promise));
            }
        };

    }  // namespace Detail

    template <typename TResult>
    class [[nodiscard]] Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> final
      : public Detail::CompletionTaskBase<TResult>
    {
    public:
        using base_type = Detail::CompletionTaskBase<TResult>;

        using value_type = base_type::value_type;
        using promise_type = base_type::promise_type;
        using handle_type = base_type::handle_type;

    public:
        explicit Task(promise_type & promise) noexcept : base_type(promise) { }

        ~Task() noexcept override = default;

        [[nodiscard]] TResult & Result() & override
        {
            this->Wait();
            return this->promise.Result();
        }

        [[nodiscard]] TResult const & Result() const & override
        {
            this->Wait();
            return this->promise.Result();
        }

        [[nodiscard]] TResult && Result() && override
        {
            this->Wait();
            return std::move(this->promise.Result());
        }

        [[nodiscard]] TResult const && Result() const && override
        {
            this->Wait();
            return std::move(this->promise.Result());
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->promise.ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    template <typename TResult>
    class [[nodiscard]] Task<TResult &, Detail::TaskCompletionSourcePromise<TResult &>> final
      : public Detail::CompletionTaskBase<TResult &>
    {
    public:
        using base_type = Detail::CompletionTaskBase<TResult &>;

        using value_type = base_type::value_type;
        using promise_type = base_type::promise_type;
        using handle_type = base_type::handle_type;

    public:
        explicit Task(promise_type & promise) noexcept : base_type(promise) { }

        ~Task() noexcept override = default;

        [[nodiscard]] TResult & Result() override
        {
            this->Wait();
            return this->promise.Result();
        }

        [[nodiscard]] TResult const & Result() const override
        {
            this->Wait();
            return this->promise.Result();
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->promise.ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

    template <>
    class [[nodiscard]] Task<void, Detail::TaskCompletionSourcePromise<void>> final
      : public Detail::CompletionTaskBase<void>
    {
    public:
        using base_type = Detail::CompletionTaskBase<void>;

        using value_type = base_type::value_type;
        using promise_type = base_type::promise_type;
        using handle_type = base_type::handle_type;

    public:
        explicit Task(promise_type & promise) noexcept : base_type(promise) { }

        ~Task() noexcept override = default;

        void ThrowIfFaulted() const override
        {
            this->Wait();
            this->promise.ThrowIfFaulted();
        }

        [[nodiscard]] Task && ContinueOn(ITaskScheduler & taskScheduler) &&
        {
            this->promise.ContinuationScheduler(&taskScheduler);
            return std::move(*this);
        }
    };

#pragma endregion

#pragma region TaskCompletionSource

    namespace Detail
    {

        template <typename TResult>
        class TaskCompletionSourceBase
        {
        protected:
            // ToDo: std::shared_ptr
            Detail::TaskCompletionSourcePromise<TResult> promise;

        public:
            TaskCompletionSourceBase() noexcept = default;

            TaskCompletionSourceBase(TaskCompletionSourceBase const &) = delete;
            TaskCompletionSourceBase & operator=(TaskCompletionSourceBase const &) = delete;

            TaskCompletionSourceBase(TaskCompletionSourceBase &&) = delete;
            TaskCompletionSourceBase & operator=(TaskCompletionSourceBase &&) = delete;

            virtual ~TaskCompletionSourceBase() noexcept = default;

            [[nodiscard]] ::TaskSystem::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>> Task()
            {
                return ::TaskSystem::Task<TResult, Detail::TaskCompletionSourcePromise<TResult>>(promise);
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

    }

    template <typename TResult>
    class TaskCompletionSource final : public Detail::TaskCompletionSourceBase<TResult>
    {
    public:
        template <typename TValue, std::enable_if_t<std::is_convertible_v<TValue &&, TResult>> * = nullptr>
        [[nodiscard]] bool TrySetResult(TValue && value) noexcept(
            std::is_nothrow_constructible_v<TResult, decltype(value)>)
        {
            return this->promise.TrySetResult(std::forward<TValue>(value));
        }

        template <typename TValue, std::enable_if_t<std::is_convertible_v<TValue &&, TResult>> * = nullptr>
        void SetResult(TValue && value)
        {
            if (!TrySetResult(std::forward<TValue>(value)))
            {
                throw std::exception("Unable to set value");
            }
        }
    };

    template <>
    class TaskCompletionSource<void> final : public Detail::TaskCompletionSourceBase<void>
    {
    public:
        [[nodiscard]] bool TrySetCompleted() noexcept
        {
            return this->promise.TrySetCompleted();
        }

        void SetCompleted()
        {
            if (!TrySetCompleted())
            {
                throw std::exception("Unable to set completed");
            }
        }
    };

}  // namespace TaskSystem