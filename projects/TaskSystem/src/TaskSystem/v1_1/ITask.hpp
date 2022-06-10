#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_1/Awaitable.hpp>


namespace TaskSystem::v1_1
{

    template <typename TResult>
    class [[nodiscard]] ITask
    {
    public:
        virtual ~ITask() noexcept = default;

        Awaitable<TResult> operator co_await() const & noexcept { return GetAwaitable(); }

        Awaitable<TResult> operator co_await() const && noexcept { return GetAwaitable(); }

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        virtual void Wait() const noexcept = 0;

        [[nodiscard]] virtual TResult & Result() & = 0;

        [[nodiscard]] virtual TResult const & Result() const & = 0;

        [[nodiscard]] virtual TResult && Result() && = 0;

        [[nodiscard]] virtual TResult const && Result() const && = 0;

        // Maybe: ScheduleOn & ContinueOn?

    protected:
        [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() const & noexcept = 0;
        [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() const && noexcept = 0;
    };

    template <>
    class [[nodiscard]] ITask<void>
    {
    public:
        virtual ~ITask() noexcept = default;

        Awaitable<void> operator co_await() const & noexcept { return GetAwaitable(); }

        Awaitable<void> operator co_await() const && noexcept { return GetAwaitable(); }

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        virtual void Wait() const noexcept = 0;

        [[nodiscard]] virtual void ThrowIfFaulted() const = 0;

        // Maybe: ScheduleOn & ContinueOn?

    protected:
        [[nodiscard]] virtual Awaitable<void> GetAwaitable() const & noexcept = 0;
        [[nodiscard]] virtual Awaitable<void> GetAwaitable() const && noexcept = 0;
    };

}  // namespace TaskSystem::v1_1