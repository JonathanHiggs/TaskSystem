#include <TaskSystem/v1_0/SynchronousTaskScheduler.hpp>


namespace TaskSystem::inline v1_0
{

    bool SynchronousTaskScheduler::IsWorkerThread() const noexcept
    {
        return true;
    }

    void SynchronousTaskScheduler::Schedule(std::coroutine_handle<> handle)
    {
        queue.push(Token{ handle });
    }

    void SynchronousTaskScheduler::Run()
    {
        while (!queue.empty())
        {
            auto token = std::move(queue.front());
            queue.pop();

            token.Handle.resume();
        }
    }

    void SynchronousTaskScheduler::ScheduleToken(Token token)
    {
        queue.push(std::move(token));
    }

}  // namespace TaskSystem::inline v1_0