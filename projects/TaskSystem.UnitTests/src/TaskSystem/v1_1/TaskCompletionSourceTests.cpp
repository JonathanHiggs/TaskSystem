#include <TaskSystem/v1_1/TaskCompletionSource.hpp>
#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{
    TEST(TaskCompletionSource, test)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();
        auto taskCompletionSource = TaskCompletionSource<int>();

        auto task = [&]() -> Task<int> {
            auto value = co_await taskCompletionSource.Task();
            co_return value;
        }();

        scheduler.Schedule(task);

        // Act & Assert
        scheduler.Run();

        EXPECT_EQ(task.State(), TaskState::Suspended);

        EXPECT_TRUE(taskCompletionSource.TrySetResult(expected));
        scheduler.Run();

        EXPECT_EQ(task.State(), TaskState::Completed);

        EXPECT_EQ(task.Result(), expected);
    }
}