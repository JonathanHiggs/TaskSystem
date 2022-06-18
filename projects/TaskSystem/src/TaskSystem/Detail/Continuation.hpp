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
        ITaskScheduler * scheduler = nullptr;

    public:
        Continuation() noexcept = default;

        Continuation(std::nullptr_t) noexcept { }

        Continuation(IPromise & promise) noexcept
          : promise(&promise), scheduler(nullptr)
        { }

        Continuation(IPromise & promise, ITaskScheduler * scheduler) noexcept
          : promise(&promise), scheduler(scheduler)
        { }

        [[nodiscard]] operator bool() const noexcept;

        [[nodiscard]] IPromise & Promise() const noexcept { return *promise; }

        [[nodiscard]] std::coroutine_handle<> Handle();

        [[nodiscard]] ITaskScheduler * Scheduler() const noexcept { return scheduler; }
    };

}  // namespace TaskSystem::Detail