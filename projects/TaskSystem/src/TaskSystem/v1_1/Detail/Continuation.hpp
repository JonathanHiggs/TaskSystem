#pragma once

#include <TaskSystem/v1_1/ITaskScheduler.hpp>

#include <coroutine>


namespace TaskSystem::v1_1::Detail
{

    class Continuation final
    {
    private:
        std::coroutine_handle<> handle;
        ITaskScheduler * scheduler;

    public:
        Continuation() noexcept : handle(nullptr), scheduler(nullptr)
        { }

        Continuation(std::coroutine_handle<> handle) noexcept : handle(handle), scheduler(nullptr)
        { }

        Continuation(std::coroutine_handle<> handle, ITaskScheduler * scheduler) noexcept
            : handle(handle), scheduler(scheduler)
        { }

        [[nodiscard]] operator bool() const noexcept
        {
            return handle != nullptr;
        }

        [[nodiscard]] std::coroutine_handle<> Handle() const noexcept
        {
            return handle;
        }

        [[nodiscard]] ITaskScheduler * Scheduler() const noexcept
        {
            return scheduler;
        }
    };

}  // namespace TaskSystem::v1_1::Detail