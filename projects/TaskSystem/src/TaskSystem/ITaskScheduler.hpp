#pragma once

#include <TaskSystem/ScheduleItem.hpp>


namespace TaskSystem
{

    class ITaskScheduler
    {
    public:
        virtual ~ITaskScheduler() noexcept = default;

        virtual bool IsWorkerThread() const noexcept = 0;

        virtual void Schedule(ScheduleItem && item) = 0;
    };

    // ToDo: Move these to ExecutionContext class
    ITaskScheduler * CurrentScheduler();

    ITaskScheduler * DefaultScheduler();

    bool IsCurrentScheduler(ITaskScheduler * scheduler);

}  // namespace TaskSystem