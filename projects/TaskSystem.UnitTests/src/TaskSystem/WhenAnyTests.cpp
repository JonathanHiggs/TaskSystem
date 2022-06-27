#include <TaskSystem/SynchronousTaskScheduler.hpp>
#include <TaskSystem/Task.hpp>
#include <TaskSystem/TaskCompletionSource.hpp>
#include <TaskSystem/WhenAny.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Tests
{

    TEST(WhenAnyTests, callerResultesWhenInnerTaskCompletes)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<>();
        auto innerTask = taskCompletionSource.Task();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(innerTask);
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

    TEST(WhenAnyTests, callerResumesWhenOneInnerTaskCompletes)
    {
        // Arrange
        auto taskCompletionSource1 = TaskCompletionSource<>();
        auto innerTask1 = taskCompletionSource1.Task();

        auto taskCompletionSource2 = TaskCompletionSource<>();
        auto innerTask2 = taskCompletionSource2.Task();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(innerTask1, innerTask2);
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        scheduler.Schedule(outerTask);
        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Suspended);

        taskCompletionSource1.SetCompleted();

        EXPECT_EQ(outerTask.State(), TaskState::Scheduled);

        scheduler.Run();

        EXPECT_EQ(outerTask.State(), TaskState::Completed);

        // ToDo: WhenAny needs to keep track of the promises it is set as the continuation of, and remove them when
        // completed
        taskCompletionSource2.SetCompleted();
    }

    TEST(WhenAnyTests, callerResumesWhenRvalueTask)
    {
        // Arrange
        auto innerTaskFn = []() -> Task<> {
            co_return;
        };

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(innerTaskFn());
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAnyTests, callerResumesWhenCompletedTask)
    {
        // Arrange
        auto innerTask = []() -> Task<> {
            co_return;
        }();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(innerTask);
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

    TEST(WhenAnyTests, callerResumesWhenCompletedTask2)
    {
        // Arrange
        auto taskCompletionSource = TaskCompletionSource<>();
        taskCompletionSource.SetCompleted();

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(taskCompletionSource.Task());
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAnyTests, callerResumesWithValueTask)
    {
        // Arrange
        auto innerTask1 = ValueTask<int>(42);
        auto innerTask2 = ValueTask<int>(42);

        auto outerTask = [&]() -> Task<> {
            co_await WhenAny(innerTask1, innerTask2);
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(outerTask);
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
    }

    TEST(WhenAnyTests, callerResumesWithTempValueTask)
    {
        // Arrange
        auto task = [&]() -> Task<> {
            co_await WhenAny(ValueTask<int>(42), ValueTask<int>(42));
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);
        scheduler.Run();

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
    }

}