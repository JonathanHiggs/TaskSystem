#include <TaskSystem/SynchronousTaskScheduler.hpp>
#include <TaskSystem/TaskCompletionSource.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Tests
{

    TEST(TaskCompletionSourceTests, returnByValue)
    {
        // Arrange
        auto expected = 42;
        auto taskCompletionSource = TaskCompletionSource<int>();
        auto task = taskCompletionSource.Task();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        taskCompletionSource.SetResult(expected);

        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    TEST(TaskCompletionSourceTests, returnByRef)
    {
        // Arrange
        auto expected = 42;
        auto taskCompletionSource = TaskCompletionSource<int &>();
        auto task = taskCompletionSource.Task();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        taskCompletionSource.SetResult(expected);

        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(&task.Result(), &expected);
    }

    TEST(TaskCompletionSourceTests, returnVoid)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<void>();
        auto task = taskCompletionSource.Task();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        taskCompletionSource.SetCompleted();

        EXPECT_EQ(task.State(), TaskState::Completed);
    }

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

    TEST(TaskCompletionSourceTests, multipleAwaitingTasks)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();

        auto taskCompletionSource = TaskCompletionSource<int>();

        auto taskFn = [&]() -> Task<int> {
            co_return co_await taskCompletionSource.Task();
        };

        auto task1 = taskFn();
        auto task2 = taskFn();
        auto task3 = taskFn();
        auto task4 = taskFn();
        auto task5 = taskFn();
        auto task6 = taskFn();

        scheduler.Schedule(task1);
        scheduler.Schedule(task2);
        scheduler.Schedule(task3);
        scheduler.Schedule(task4);
        scheduler.Schedule(task5);
        scheduler.Schedule(task6);

        // Act & Assert
        scheduler.Run();

        EXPECT_EQ(task1.State(), TaskState::Suspended);
        EXPECT_EQ(task2.State(), TaskState::Suspended);
        EXPECT_EQ(task3.State(), TaskState::Suspended);
        EXPECT_EQ(task4.State(), TaskState::Suspended);
        EXPECT_EQ(task5.State(), TaskState::Suspended);
        EXPECT_EQ(task6.State(), TaskState::Suspended);

        taskCompletionSource.SetResult(expected);

        EXPECT_EQ(task1.State(), TaskState::Scheduled);
        EXPECT_EQ(task2.State(), TaskState::Scheduled);
        EXPECT_EQ(task3.State(), TaskState::Scheduled);
        EXPECT_EQ(task4.State(), TaskState::Scheduled);
        EXPECT_EQ(task5.State(), TaskState::Scheduled);
        EXPECT_EQ(task6.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(task1.State(), TaskState::Completed);
        EXPECT_EQ(task1.Result(), expected);

        EXPECT_EQ(task2.State(), TaskState::Completed);
        EXPECT_EQ(task2.Result(), expected);

        EXPECT_EQ(task3.State(), TaskState::Completed);
        EXPECT_EQ(task3.Result(), expected);

        EXPECT_EQ(task4.State(), TaskState::Completed);
        EXPECT_EQ(task4.Result(), expected);

        EXPECT_EQ(task5.State(), TaskState::Completed);
        EXPECT_EQ(task5.Result(), expected);

        EXPECT_EQ(task6.State(), TaskState::Completed);
        EXPECT_EQ(task6.Result(), expected);
    }

}  // namespace TaskSystem::Tests