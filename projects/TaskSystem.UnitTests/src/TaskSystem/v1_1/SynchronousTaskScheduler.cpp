#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{

    TEST(SynchronousTaskSchedulerTests_v1_1, runWithLambdas)
    {
        // Arrange
        auto completed1 = false;
        auto completed2 = false;

        auto task1 = [&]() { completed1 = true; };
        auto task2 = [&]() { completed2 = true; };

        auto scheduler = SynchronousTaskScheduler();

        scheduler.Schedule(ScheduleItem(std::move(task1)));
        scheduler.Schedule(ScheduleItem(std::move(task2)));

        // Act
        scheduler.Run();

        // Assert
        EXPECT_TRUE(completed1);
        EXPECT_TRUE(completed2);
    }

}