#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_1/Awaitable.hpp>


namespace TaskSystem::v1_1
{
    namespace Detail
    {

        template <typename TResult>
        class ITaskBase
        {
        public:
            virtual ~ITaskBase() noexcept = default;

            Awaitable<TResult> operator co_await() const & noexcept { return GetAwaitable(); }
            Awaitable<TResult> operator co_await() const && noexcept { return GetAwaitable(); }

            [[nodiscard]] virtual TaskState State() const noexcept = 0;

            virtual void Wait() const noexcept = 0;

            void ScheduleOn(ITaskScheduler & taskScheduler) &;
            void ContinueOn(ITaskScheduler & taskScheduler) &;

        protected:
            [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() const & noexcept = 0;
            [[nodiscard]] virtual Awaitable<TResult> GetAwaitable() const && noexcept = 0;
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
        [[nodiscard]] virtual void ThrowIfFaulted() const = 0;
    };

}  // namespace TaskSystem::v1_1