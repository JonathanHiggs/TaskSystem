#pragma once

#include <TaskSystem/Detail/Continuations.hpp>
#include <TaskSystem/TaskState.hpp>

#include <exception>


class TaskSystem::ITaskScheduler;


namespace TaskSystem::Detail
{

    struct IgnoreAlreadySetTag
    {
    };

    constexpr IgnoreAlreadySetTag IgnoreAlreadySet{};

    class IPromise
    {
    public:
        virtual ~IPromise() noexcept = default;

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        [[nodiscard]] virtual std::coroutine_handle<> Handle() noexcept = 0;

        // Maybe: void Resume() ?

        [[nodiscard]] virtual Detail::Continuations & Continuations() noexcept = 0;
        [[nodiscard]] virtual bool TryAddContinuation(Detail::Continuation value) noexcept = 0;

        [[nodiscard]] virtual ITaskScheduler * ContinuationScheduler() const noexcept = 0;
        virtual void ContinuationScheduler(ITaskScheduler * value) noexcept = 0;

        [[nodiscard]] virtual ITaskScheduler * TaskScheduler() const noexcept { return nullptr; }
        virtual void TaskScheduler(ITaskScheduler * value) noexcept { }

        [[nodiscard]] virtual bool TrySetScheduled() noexcept = 0;

        [[nodiscard]] virtual bool TrySetRunning() noexcept = 0;
        // Maybe: Move to PromisePolicy::AllowSetRunningWhenRunning
        [[nodiscard]] virtual bool TrySetRunning(IgnoreAlreadySetTag) noexcept = 0;

        [[nodiscard]] virtual bool TrySetSuspended() noexcept = 0;

        // ToDo: TrySetCancelled;

        [[nodiscard]] virtual bool TrySetException(std::exception_ptr ex) noexcept = 0;

        virtual void Wait() const noexcept = 0;

        virtual void ScheduleContinuations() noexcept = 0;
    };

    template <typename T>
    concept PromiseType = std::derived_from<T, IPromise>;

}  // namespace TaskSystem::Detail