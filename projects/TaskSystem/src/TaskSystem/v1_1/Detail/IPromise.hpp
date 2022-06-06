#pragma once

#include <TaskSystem/TaskState.hpp>
#include <TaskSystem/v1_1/Detail/Continuation.hpp>
#include <TaskSystem/v1_1/ITaskScheduler.hpp>

#include <exception>


namespace TaskSystem::v1_1::Detail
{

    class IPromise
    {
    public:
        virtual ~IPromise() noexcept = default;

        [[nodiscard]] virtual TaskState State() const noexcept = 0;

        [[nodiscard]] virtual Detail::Continuation const & Continuation() const noexcept = 0;

        // ToDo: TryAddContinuation
        [[nodiscard]] virtual bool TrySetContinuation(Detail::Continuation value) noexcept = 0;

        [[nodiscard]] virtual ITaskScheduler * ContinuationScheduler() const noexcept = 0;

        virtual void ContinuationScheduler(ITaskScheduler * value) noexcept = 0;

        [[nodiscard]] virtual bool TrySetScheduled() noexcept = 0;

        [[nodiscard]] virtual bool TrySetRunning() noexcept = 0;

        [[nodiscard]] virtual bool TrySetSuspended() noexcept = 0;

        // ToDo: TrySetCancelled;

        [[nodiscard]] virtual bool TrySetException(std::exception_ptr ex) noexcept = 0;

        virtual void Wait() const noexcept = 0;
    };

    template <typename T>
    concept PromiseType = std::derived_from<T, IPromise>;

}  // namespace TaskSystem::v1_1::Detail