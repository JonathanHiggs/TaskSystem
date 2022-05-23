#pragma once

#include <TaskSystem/v1_1/ScheduleItem.hpp>


namespace TaskSystem::v1_1
{

    class ITaskScheduler
    {
    public:
        virtual ~ITaskScheduler() noexcept = default;

        virtual bool IsWorkerThread() const noexcept = 0;

        virtual void Schedule(ScheduleItem && item) = 0;
    };

}  // namespace TaskSystem::v1_1