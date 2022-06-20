#include <TaskSystem/SynchronousTaskScheduler.hpp>
#include <TaskSystem/Task.hpp>
#include <TaskSystem/TaskCompletionSource.hpp>
#include <TaskSystem/WhenAll.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Tests
{

    TEST(WhenAllTests, callerResumesWhenInnerTaskCompletes)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<>();
        auto innerTask = taskCompletionSource.Task();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(innerTask);
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        scheduler.Schedule(outerTask);
        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Suspended);

        taskCompletionSource.SetCompleted();

        EXPECT_EQ(outerTask.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWhenMultipleInnerTaskCompletes)
    {
        // Arrange
        auto taskCompletionSource1 = TaskCompletionSource<>();
        auto innerTask1 = taskCompletionSource1.Task();

        auto taskCompletionSource2 = TaskCompletionSource<>();
        auto innerTask2 = taskCompletionSource2.Task();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(innerTask1, innerTask2);
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        scheduler.Schedule(outerTask);
        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Suspended);

        taskCompletionSource1.SetCompleted();
        EXPECT_EQ(outerTask.State(), TaskState::Suspended);

        taskCompletionSource2.SetCompleted();
        EXPECT_EQ(outerTask.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWhenRvalueTask)
    {
        // Arrange
        auto innerTaskFn = []() -> Task<> {
            co_return;
        };

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(innerTaskFn());
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWhenCompletedTask)
    {
        // Arrange
        auto innerTask = []() -> Task<> {
            co_return;
        }();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(innerTask);
        }();

        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(innerTask);
        scheduler.Run();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWhenCompletedTask2)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<>();
        taskCompletionSource.SetCompleted();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(taskCompletionSource.Task());
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWithValueTask)
    {
        // Arrange
        auto innerTask1 = ValueTask<int>(42);
        auto innerTask2 = ValueTask<int>(42);

        auto outerTask = [&]() -> Task<> {
            co_await WhenAll(innerTask1, innerTask2);
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAllTests, callerResumesWithTempValueTask)
    {
        // Arrange
        auto task = [&]() -> Task<> {
            co_await WhenAll(ValueTask<int>(42), ValueTask<int>(42));
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);
        scheduler.Run();

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
    }

}