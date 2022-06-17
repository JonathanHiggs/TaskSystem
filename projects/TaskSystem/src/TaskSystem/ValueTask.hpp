#pragma once

#include <TaskSystem/Awaitable.hpp>
#include <TaskSystem/Detail/IPromise.hpp>
#include <TaskSystem/ITask.hpp>
#include <TaskSystem/TaskState.hpp>

#include <coroutine>


namespace TaskSystem
{
    namespace Detail
    {

        template <typename TResult>
        class ValueTaskAwaitable
        {
        private:
            TResult * result;

        public:
            explicit ValueTaskAwaitable(TResult * result) noexcept : result(result) { }

            constexpr bool await_ready() const noexcept { return true; }
            constexpr void await_suspend(std::coroutine_handle<>) const noexcept { }
            constexpr TResult await_resume() const noexcept { return *result; }
        };

        template <typename TResult>
        class ValueTaskAwaitable<TResult &>
        {
        private:
            TResult * result;

        public:
            explicit ValueTaskAwaitable(TResult & result) noexcept : result(&result) { }

            constexpr bool await_ready() const noexcept { return true; }
            constexpr void await_suspend(std::coroutine_handle<>) const noexcept { }
            constexpr TResult & await_resume() const { return *result; }
        };

        template <>
        class ValueTaskAwaitable<void>
        {
        public:
            constexpr bool await_ready() const noexcept { return true; }
            constexpr void await_suspend(std::coroutine_handle<>) const noexcept { }
            constexpr void await_resume() const noexcept { }
        };

    }  // namespace Detail

    template <typename TResult>
    class [[nodiscard]] ValueTask final : public ITask<TResult>
    {
    public:
        using value_type = TResult;
        using promise_type = void;
        using handle_type = void;

    private:
        TResult result;

    public:
        explicit ValueTask(TResult const & result) noexcept(std::is_nothrow_copy_constructible_v<TResult>)
          : result(result)
        { }

        explicit ValueTask(TResult && result) noexcept(std::is_nothrow_move_constructible_v<TResult>)
          : result(std::move(result))
        { }

        auto operator co_await() & noexcept { return Detail::ValueTaskAwaitable<TResult>(&result); }
        auto operator co_await() && noexcept { return Detail::ValueTaskAwaitable<TResult>(&result); }

        [[nodiscard]] TaskState State() const noexcept override { return TaskState::Completed; }

        [[nodiscard]] TResult & Result() & override { return result; }
        [[nodiscard]] TResult const & Result() const & override { return result; }
        [[nodiscard]] TResult && Result() && override { return std::move(result); }
        [[nodiscard]] TResult const && Result() const && override { return std::move(result); }

        void Wait() const noexcept override { }

        void ScheduleOn(ITaskScheduler & taskScheduler) & override { }
        void ContinueOn(ITaskScheduler & taskScheduler) & override { }

    protected:
        [[nodiscard]] Awaitable<TResult> GetAwaitable() & noexcept override
        {
            return Awaitable<TResult>(Detail::ValueTaskAwaitable<TResult>(&result));
        }

        [[nodiscard]] Awaitable<TResult> GetAwaitable() && noexcept override
        {
            return Awaitable<TResult>(Detail::ValueTaskAwaitable<TResult>(&result));
        }
    };

    template <typename TResult>
    class [[nodiscard]] ValueTask<TResult &> final : public ITask<TResult &>
    {
    public:
        using value_type = TResult &;
        using promise_type = void;
        using handle_type = void;

    private:
        TResult result;

    public:
        explicit ValueTask(TResult const & result) noexcept(std::is_nothrow_copy_constructible_v<TResult>)
          : result(result)
        { }

        explicit ValueTask(TResult && result) noexcept(std::is_nothrow_move_constructible_v<TResult>)
          : result(std::move(result))
        { }

        auto operator co_await() const & noexcept { return Detail::ValueTaskAwaitable<TResult &>(result); }
        auto operator co_await() const && noexcept { return Detail::ValueTaskAwaitable<TResult &>(result); }

        [[nodiscard]] TaskState State() const noexcept override { return TaskState::Completed; }

        [[nodiscard]] TResult & Result() & override { return result; }
        [[nodiscard]] TResult const & Result() const & override { return result; }
        [[nodiscard]] TResult && Result() && override { return std::move(result); }
        [[nodiscard]] TResult const && Result() const && override { return std::move(result); }

        void Wait() const noexcept override { }

        void ScheduleOn(ITaskScheduler & taskScheduler) & override { }
        void ContinueOn(ITaskScheduler & taskScheduler) & override { }

    protected:
        [[nodiscard]] Awaitable<TResult> GetAwaitable() & noexcept override
        {
            return Awaitable<TResult>(Detail::ValueTaskAwaitable<TResult &>(result));
        }

        [[nodiscard]] Awaitable<TResult> GetAwaitable() && noexcept override
        {
            return Awaitable<TResult>(Detail::ValueTaskAwaitable<TResult &>(result));
        }
    };

    template <>
    class [[nodiscard]] ValueTask<void> final : public ITask<void>
    {
    public:
        using value_type = void;
        using promise_type = void;
        using handle_type = void;

    public:
        ValueTask() noexcept { }

        auto operator co_await() const & noexcept { return Detail::ValueTaskAwaitable<void>(); }
        auto operator co_await() const && noexcept { return Detail::ValueTaskAwaitable<void>(); }

        [[nodiscard]] TaskState State() const noexcept override { return TaskState::Completed; }

        [[nodiscard]] void ThrowIfFaulted() const override { }

        void Wait() const noexcept override { }

        void ScheduleOn(ITaskScheduler & taskScheduler) & override { }
        void ContinueOn(ITaskScheduler & taskScheduler) & override { }

    protected:
        [[nodiscard]] Awaitable<void> GetAwaitable() & noexcept override
        {
            return Awaitable<void>(Detail::ValueTaskAwaitable<void>());
        }

        [[nodiscard]] Awaitable<void> GetAwaitable() && noexcept override
        {
            return Awaitable<void>(Detail::ValueTaskAwaitable<void>());
        }
    };

}  // namespace TaskSystem