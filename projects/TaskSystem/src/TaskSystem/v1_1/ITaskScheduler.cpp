#include <TaskSystem/v1_1/ITaskScheduler.hpp>


namespace TaskSystem::v1_1
{

    static thread_local ITaskScheduler * current;

    void SetCurrentScheduler(ITaskScheduler * scheduler)
    {
        current = scheduler;
    }

    ITaskScheduler * CurrentScheduler()
    {
        return current;
    }

    ITaskScheduler * DefaultScheduler()
    {
        // ToDo: return global TaskScheduler instance
        return nullptr;
    }

    bool IsCurrentScheduler(ITaskScheduler * scheduler)
    {
        return scheduler == CurrentScheduler();
    }

}  // namespace TaskSystem::v1_1