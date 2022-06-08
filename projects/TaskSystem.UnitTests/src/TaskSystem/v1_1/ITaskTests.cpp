#include <TaskSystem/v1_1/ITask.hpp>
#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>
#include <TaskSystem/v1_1/Task.hpp>
#include <TaskSystem/v1_1/TaskCompletionSource.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{

    TEST(ITaskTests, taskAsITask)
    {
        // Arrange
        auto scheduler = SynchronousTaskScheduler();
        auto expected = 42;
        auto task = [&]() -> Task<int> {
            co_return expected;
        }();

        ITask<int> & iTask = task;

        // Act
        scheduler.Schedule(task);
        scheduler.Run();

        // Assert
        EXPECT_EQ(iTask.Result(), expected);
    }

    TEST(ITaskTests, awaitTaskAsITask)
    {
        // Arrange
        auto expected = 42;
        auto task = [&]() -> Task<int> { co_return expected; }();

        ITask<int> & iTask = task;

        auto outerTask = [&]() -> Task<int> { co_return co_await iTask; }();
        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(outerTask);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.Result(), expected);
    }

    TEST(ITaskTests, completionTaskAsITask)
    {
        // Arrange
        auto expected = 42;
        auto taskCompletionSource = TaskCompletionSource<int>();
        auto task = taskCompletionSource.Task();

        ITask<int> & iTask = task;

        // Arrange
        taskCompletionSource.SetResult(expected);

        // Assert
        EXPECT_EQ(iTask.Result(), expected);
    }

    TEST(ITaskTests, awaitCompletionTaskAsITask)
    {
        // Arrange
        auto expected = 42;
        auto taskCompletionSource = TaskCompletionSource<int>();
        auto task = taskCompletionSource.Task();

        ITask<int>& iTask = task;

        auto outerTask = [&]() -> Task<int> { co_return co_await iTask; }();
        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(outerTask);

        // Act
        taskCompletionSource.SetResult(expected);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.Result(), expected);
    }

}  // namespace TaskSystem::v1_1::Tests