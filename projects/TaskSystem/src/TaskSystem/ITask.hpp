#pragma once

#include <TaskSystem/Awaitable.hpp>
#include <TaskSystem/ITaskScheduler.hpp>
#include <TaskSystem/TaskState.hpp>


namespace TaskSystem
{
    namespace Detail
    {

        template <typename TResult>
        class ITaskBase
        {
        public:
            virtual ~ITaskBase() noexcept = default;

            Awaitable<TResult> operator co_await() & noexcept { return GetAwaitable(); }
            Awaitable<TResult> operator co_await() && noexcept { return GetAwaitable(); }

            [[nodiscard]] virtual TaskState State() const noexcept = 0;

            virtual void Wait() const noexcept = 0;

            virtual void ScheduleOn(ITaskScheduler & taskScheduler) & = 0;
            virtual void ContinueOn(ITaskScheduler & taskScheduler) & = 0;

        protected:
            [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() & noexcept = 0;
            [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() && noexcept = 0;
        };

    }  // namespace Detail

    template <typename TResult>
    class [[nodiscard]] ITask : public Detail::ITaskBase<TResult>
    {
    public:
        [[nodiscard]] virtual TResult & Result() & = 0;
        [[nodiscard]] virtual TResult const & Result() const & = 0;
        [[nodiscard]] virtual TResult && Result() && = 0;
        [[nodiscard]] virtual TResult const && Result() const && = 0;
    };

    template <typename TResult>
    class [[nodiscard]] ITask<TResult &> : public Detail::ITaskBase<TResult &>
    {
    public:
        [[nodiscard]] virtual TResult & Result() = 0;
        [[nodiscard]] virtual TResult const & Result() const = 0;
    };

    template <>
    class [[nodiscard]] ITask<void> : public Detail::ITaskBase<void>
    {
    public:
        // Maybe: void Result() = 0; ?
        [[nodiscard]] virtual void ThrowIfFaulted() const = 0;
    };

}  // namespace TaskSystem