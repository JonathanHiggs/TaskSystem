#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>
#include <TaskSystem/v1_1/TaskCompletionSource.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{

    TEST(TaskCompletionSourceTests, trySetResultCompletesTask)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();
        auto taskCompletionSource = TaskCompletionSource<int>();
        auto innerTask = taskCompletionSource.Task();

        auto task = [&]() -> Task<int> {
            auto value = co_await innerTask;
            co_return value;
        }();

        scheduler.Schedule(task);

        // Act & Assert
        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Suspended);

        EXPECT_TRUE(taskCompletionSource.TrySetResult(expected));

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.Result(), expected);

        scheduler.Run();

        EXPECT_EQ(task.State(), TaskState::Completed);

        EXPECT_EQ(task.Result(), expected);
    }

    TEST(TaskCompletionSourceTests, trySetExceptionFaultsTask)
    {
        // Arrange
        using expected = std::exception;
        auto scheduler = SynchronousTaskScheduler();
        auto taskCompletionSource = TaskCompletionSource<int>();
        auto innerTask = taskCompletionSource.Task();

        auto task = [&]() -> Task<int> {
            auto value = co_await innerTask;
            co_return value;
        }();

        scheduler.Schedule(task);

        // Act & Assert
        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Suspended);

        EXPECT_TRUE(taskCompletionSource.TrySetException(expected()));

        EXPECT_EQ(innerTask.State(), TaskState::Error);
        EXPECT_THROW(innerTask.Result(), expected);

        scheduler.Run();

        EXPECT_EQ(task.State(), TaskState::Error);

        EXPECT_THROW(task.Result(), expected);
    }

    TEST(TaskCompletionSourceTests, setExceptionAfterSetResultThrows)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetResult(42);

        // Act & Assert
        EXPECT_THROW(taskCompletionSource.SetException(std::exception()), std::exception);
    }

    TEST(TaskCompletionSourceTests, trySetExceptionAfterSetResultReturnsFalse)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetResult(42);

        // Act & Assert
        EXPECT_FALSE(taskCompletionSource.TrySetException(std::exception()));
    }

    TEST(TaskCompletionSourceTests, setResultAfterSetExceptionThrows)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetException(std::exception());

        // Act & Assert
        EXPECT_THROW(taskCompletionSource.SetResult(42), std::exception);
    }

    TEST(TaskCompletionSourceTests, trySetResultAfterSetExceptionReturnsFalse)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetException(std::exception());

        // Act & Assert
        EXPECT_FALSE(taskCompletionSource.TrySetResult(42));
    }

    TEST(TaskCompletionSourceTests, setResultTwiceThrows)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetResult(42);

        // Act & Assert
        EXPECT_THROW(taskCompletionSource.SetResult(42), std::exception);
    }

    TEST(TaskCompletionSourceTests, trySetResultTwiceReturnsFalse)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetResult(42);

        // Act & Assert
        EXPECT_FALSE(taskCompletionSource.TrySetResult(42));
    }

    TEST(TaskCompletionSourceTests, setExceptionTwiceThrows)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetException(42);

        // Act & Assert
        EXPECT_THROW(taskCompletionSource.SetException(42), std::exception);
    }

    TEST(TaskCompletionSourceTests, trySetExceptionTwiceReturnsFalse)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<int>();
        taskCompletionSource.SetException(42);

        // Act & Assert
        EXPECT_FALSE(taskCompletionSource.TrySetException(42));
    }

}  // namespace TaskSystem::v1_1::Tests