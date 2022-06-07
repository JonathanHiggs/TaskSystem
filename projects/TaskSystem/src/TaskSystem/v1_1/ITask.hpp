#pragma once

#include <TaskSystem/TaskState.hpp>


namespace TaskSystem::v1_1
{

    template <typename TResult>
    class [[nodiscard]] ITask
    {
    public:
        virtual ~ITask() noexcept = default;

        // ToDo:
        // virtual IAwaitable operator co_await() const & noexcept;
        // virtual IAwaitable operator co_await() const && noexcept;

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        virtual void Wait() const noexcept = 0;

        [[nodiscard]] virtual TResult & Result() & = 0;

        [[nodiscard]] virtual TResult const & Result() const & = 0;

        [[nodiscard]] virtual TResult && Result() && = 0;

        [[nodiscard]] virtual TResult const && Result() const && = 0;
    };

    template <>
    class [[nodiscard]] ITask<void>
    {
    public:
        virtual ~ITask() noexcept = default;

        // ToDo:
        // virtual IAwaitable operator co_await() const & noexcept;
        // virtual IAwaitable operator co_await() const && noexcept;

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        virtual void Wait() const noexcept = 0;

        [[nodiscard]] virtual void ThrowIfFaulted() const = 0;
    };

}  // namespace TaskSystem::v1_1