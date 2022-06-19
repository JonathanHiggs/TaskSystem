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

}