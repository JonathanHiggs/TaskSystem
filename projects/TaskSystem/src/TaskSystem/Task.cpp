#include <TaskSystem/Task.hpp>


namespace TaskSystem::inline v1_0
{
    namespace Detail
    {

        TaskInitialSuspend::TaskInitialSuspend(promise_type & promise) noexcept : promise(promise)
        { }

        bool TaskInitialSuspend::await_ready() const noexcept
        {
            auto * taskScheduler = promise.TaskScheduler();
            return taskScheduler && taskScheduler->IsWorkerThread();
        }

        void TaskInitialSuspend::await_suspend(std::coroutine_handle<>) const noexcept
        {
            auto * taskScheduler = promise.TaskScheduler();
            if (!taskScheduler)
            {
                return;
            }

            auto handle = handle_type::from_promise(promise);
            // ToDo: promise.SetScheduled();
            taskScheduler->Schedule(handle);
        }

        TaskFinalSuspend::TaskFinalSuspend(promise_type & promise) noexcept : promise(promise)
        { }

        std::coroutine_handle<> TaskFinalSuspend::await_suspend(std::coroutine_handle<>) const noexcept
        {
            auto continuation = promise.Continuation();
            if (!continuation)
            {
                return std::noop_coroutine();
            }

            auto * taskScheduler = promise.ContinuationTaskScheduler();
            if (!taskScheduler || taskScheduler->IsWorkerThread())
            {
                // ToDo: promise.SetRunning();
                return continuation;
            }

            // ToDo: promise.SetScheduled();
            taskScheduler->Schedule(continuation);

            return std::noop_coroutine();
        }

        TaskPromiseBase::TaskPromiseBase() noexcept
            : continuation(nullptr), continuationTaskScheduler(nullptr), taskScheduler(nullptr)
        {
            resultReady.clear(std::memory_order::relaxed);
        }

        TaskInitialSuspend TaskPromiseBase::initial_suspend() noexcept
        {
            return TaskInitialSuspend(*this);
        }

        TaskFinalSuspend TaskPromiseBase::final_suspend() noexcept
        {
            resultReady.test_and_set();  // Maybe: can this be memory_order_relaxed?
            resultReady.notify_all();

            return TaskFinalSuspend(*this);
        }

        std::coroutine_handle<> TaskPromiseBase::Continuation() const noexcept
        {
            return continuation;
        }

        void TaskPromiseBase::Continuation(std::coroutine_handle<> value)
        {
            if (continuation != nullptr)
            {
                // throw std::exception("Multiple continuations for a single promise");
            }

            continuation = value;
        }

        ITaskScheduler * TaskPromiseBase::ContinuationTaskScheduler() const noexcept
        {
            return continuationTaskScheduler;
        }

        void TaskPromiseBase::ContinuationTaskScheduler(ITaskScheduler * value) noexcept
        {
            continuationTaskScheduler = value;
        }

        ITaskScheduler * TaskPromiseBase::TaskScheduler() const noexcept
        {
            return taskScheduler;
        }

        void TaskPromiseBase::TaskScheduler(ITaskScheduler * value) noexcept
        {
            taskScheduler = value;
        }

        void TaskPromiseBase::Wait() const noexcept
        {
            resultReady.wait(false);
        }

    }  // namespace Detail
}  // namespace TaskSystem::inline v1_0
