#include <TaskSystem/ITaskScheduler.hpp>


namespace TaskSystem
{

    static thread_local ITaskScheduler * current;

    void SetCurrentScheduler(ITaskScheduler * scheduler) { current = scheduler; }

    ITaskScheduler * CurrentScheduler() { return current; }

    ITaskScheduler * DefaultScheduler()
    {
        // ToDo: return global TaskScheduler instance
        return nullptr;
    }

    bool IsCurrentScheduler(ITaskScheduler * scheduler) { return scheduler == CurrentScheduler(); }

}  // namespace TaskSystem