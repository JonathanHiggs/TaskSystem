#pragma once

#include <TaskSystem/v1_1/ITaskScheduler.hpp>

#include <optional>
#include <queue>
#include <thread>


namespace TaskSystem::v1_1
{

    class SynchronousTaskScheduler final : public ITaskScheduler
    {
    private:
        std::optional<std::thread::id> id;
        std::queue<ScheduleItem> queue;

    public:
        SynchronousTaskScheduler() noexcept;

        ~SynchronousTaskScheduler() noexcept override = default;

        bool IsWorkerThread() const noexcept override;

        void Schedule(ScheduleItem && item) override;

        void Run();
    };

}  // namespace TaskSystem::v1_1