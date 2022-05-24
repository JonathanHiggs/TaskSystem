#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>


namespace TaskSystem::v1_1
{

    void SetCurrentScheduler(ITaskScheduler * scheduler);

    SynchronousTaskScheduler::SynchronousTaskScheduler() noexcept : id(std::this_thread::get_id())
    { }

    bool SynchronousTaskScheduler::IsWorkerThread() const noexcept
    {
        return std::this_thread::get_id() == id;
    }

    void SynchronousTaskScheduler::Schedule(ScheduleItem && item)
    {
        queue.push(std::move(item));
    }

    void SynchronousTaskScheduler::Run()
    {
        SetCurrentScheduler(this);

        while (!queue.empty())
        {
            auto item = std::move(queue.front());
            queue.pop();

            auto result = item.Run();

            if (result != nullptr)
            {
                // ToDo: unhandled exception
            }
        }

        SetCurrentScheduler(nullptr);
    }

}  // namespace TaskScheduler::v1_1