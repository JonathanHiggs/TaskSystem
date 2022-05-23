#pragma once

#include <coroutine>


namespace TaskSystem::inline v1_0
{

    struct Token
    {
        std::coroutine_handle<> Handle;
    };

    class ITaskScheduler
    {
    public:
        virtual ~ITaskScheduler() noexcept = default;

        virtual bool IsWorkerThread() const noexcept = 0;

        virtual void Schedule(std::coroutine_handle<> handle) = 0;

    protected:
        virtual void ScheduleToken(Token token) = 0;
    };

}  // namespace TaskSystem::inline v1_0