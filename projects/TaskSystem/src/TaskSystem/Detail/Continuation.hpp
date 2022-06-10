#pragma once

#include <TaskSystem/ITaskScheduler.hpp>

#include <coroutine>


namespace TaskSystem::Detail
{

    class IPromise;

    class Continuation final
    {
    private:
        IPromise * promise = nullptr;
        std::coroutine_handle<> handle = nullptr;
        ITaskScheduler * scheduler = nullptr;

    public:
        Continuation() noexcept { }

        Continuation(std::nullptr_t) noexcept { }

        Continuation(IPromise * promise, std::coroutine_handle<> handle) noexcept
          : promise(promise), handle(handle), scheduler(nullptr)
        { }

        Continuation(IPromise * promise, std::coroutine_handle<> handle, ITaskScheduler * scheduler) noexcept
          : promise(promise), handle(handle), scheduler(scheduler)
        { }

        [[nodiscard]] operator bool() const noexcept { return handle != nullptr; }

        [[nodiscard]] IPromise * Promise() const noexcept { return promise; }

        [[nodiscard]] std::coroutine_handle<> Handle() const noexcept { return handle; }

        [[nodiscard]] ITaskScheduler * Scheduler() const noexcept { return scheduler; }
    };

}  // namespace TaskSystem::Detail