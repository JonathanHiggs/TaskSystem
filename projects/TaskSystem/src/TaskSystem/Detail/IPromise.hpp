#pragma once

#include <TaskSystem/Detail/AddContinuationResult.hpp>
#include <TaskSystem/Detail/Continuations.hpp>
//#include <TaskSystem/Detail/SetCompletedResult.hpp>
//#include <TaskSystem/Detail/SetFaultedResult.hpp>
#include <TaskSystem/Detail/SetRunningResult.hpp>
#include <TaskSystem/Detail/SetScheduledResult.hpp>
//#include <TaskSystem/Detail/SetSuspendedResult.hpp>
#include <TaskSystem/TaskState.hpp>

#include <exception>


class TaskSystem::ITaskScheduler;


namespace TaskSystem::Detail
{

    class IPromise
    {
    public:
        virtual ~IPromise() noexcept = default;

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        [[nodiscard]] virtual std::coroutine_handle<> Handle() noexcept = 0;

        // Maybe: void Resume() ?
        // if promises know how to run themselves it would collapse schedule item down and could have std::function
        // and function pointer wrappers to make them accepted in places like WhenAll

        [[nodiscard]] virtual Detail::Continuations & Continuations() noexcept = 0;

        [[nodiscard]] virtual AddContinuationResult TryAddContinuation(Detail::Continuation value) noexcept = 0;

        [[nodiscard]] virtual ITaskScheduler * ContinuationScheduler() const noexcept = 0;
        virtual void ContinuationScheduler(ITaskScheduler * value) noexcept = 0;

        [[nodiscard]] virtual ITaskScheduler * TaskScheduler() const noexcept { return nullptr; }
        virtual void TaskScheduler(ITaskScheduler * value) noexcept { }

        [[nodiscard]] virtual SetScheduledResult TrySetScheduled() noexcept = 0;

        [[nodiscard]] virtual SetRunningResult TrySetRunning() noexcept = 0;

        [[nodiscard]] virtual bool TrySetSuspended() noexcept = 0;

        // ToDo: TrySetCancelled;

        [[nodiscard]] virtual bool TrySetException(std::exception_ptr ex) noexcept = 0;

        virtual void Wait() const noexcept = 0;

        virtual void ScheduleContinuations() noexcept = 0;
    };

    template <typename T>
    concept PromiseType = std::derived_from<T, IPromise>;

}  // namespace TaskSystem::Detail