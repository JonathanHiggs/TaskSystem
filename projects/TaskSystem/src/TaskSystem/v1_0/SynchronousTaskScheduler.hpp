#pragma once

#include <TaskSystem/v1_0/ITaskScheduler.hpp>

#include <queue>


namespace TaskSystem::inline v1_0
{

    class SynchronousTaskScheduler final : public ITaskScheduler
    {
    private:
        std::queue<Token> queue;

    public:
        ~SynchronousTaskScheduler() noexcept override = default;

        bool IsWorkerThread() const noexcept override;

        void Schedule(std::coroutine_handle<> handle) override;

        void Run();

    protected:
        void ScheduleToken(Token token) override;
    };

}  // namespace TaskSystem::inline v1_0