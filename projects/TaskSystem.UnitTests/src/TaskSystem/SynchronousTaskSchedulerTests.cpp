#include <TaskSystem/SynchronousTaskScheduler.hpp>

#include <TaskSystem/Utils/TestTaskFactory.hpp>

#include <gtest/gtest.h>


using TaskSystem::Utils::EmptyTask;
using TaskSystem::Utils::CopyResult;


namespace TaskSystem::Tests
{

    TEST(SynchronousTaskSchedulerTests, scheduledTaskCompletes)
    {
        // Arrange
        auto completed = false;
        auto task = Utils::FromLambda([&]() { completed = true; });

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);
        scheduler.Run();

        // Assert
        EXPECT_TRUE(completed);
    }

    TEST(SynchronousTaskSchedulerTests, taskScheduleOnCompletes)
    {
        // Arrange
        auto scheduler = SynchronousTaskScheduler();

        auto completed = false;
        auto task = Utils::FromLambda([&]() { completed = true; });
        task.ScheduleOn(scheduler);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_TRUE(completed);
    }

}  // namespace TaskSystem::Tests