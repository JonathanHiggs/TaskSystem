#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>
#include <TaskSystem/v1_1/Task.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{

    TEST(TaskTests_v1_1, taskFromLambda)
    {
        // Arrange
        auto started = false;
        auto task = Task<int>::From([&]() {
            started = true;
            return 42;
        });

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        scheduler.Schedule(task);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
    }

    TEST(TaskTests_v1_1, awaitedTaskInTask)
    {
        // Arrange
        auto started = false;
        auto expected = 42;

        auto innerTask = Task<int>::From([&]() {
            started = true;
            return expected;
        });

        auto task = [&]() -> Task<int> {
            auto inner = co_await innerTask;
            EXPECT_EQ(inner, expected);
            co_return inner;
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        scheduler.Schedule(task);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
    }

    TEST(TaskTests_v1_1, scheduleTaskTwiceThrows)
    {
        // Arrange
        auto task = Task<int>::From([]() { return 42; });

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);

        // Assert
        EXPECT_THROW(scheduler.Schedule(task), std::exception);
    }

    TEST(TaskTests_v1_1, taskScheduledOnDifferentScheduler)
    {
        // Arrange
        auto scheduler1 = SynchronousTaskScheduler();
        auto scheduler2 = SynchronousTaskScheduler();

        auto task1Completed = false;
        auto task1 = Task<int>::From([&]() {
                         EXPECT_EQ(CurrentScheduler(), &scheduler1);
                         task1Completed = true;
                         return 42;
                     })
                         .ScheduleOn(scheduler1)
                         .ContinueOn(scheduler2);

        auto task2Started = false;
        auto task2Completed = false;

        auto task2 = [&]() -> Task<int> {
            task2Started = true;
            EXPECT_EQ(CurrentScheduler(), &scheduler2);
            co_await task1;
            // co_await task1.ScheduleOn(scheduler1).ContinueOn(scheduler2);
            EXPECT_EQ(CurrentScheduler(), &scheduler2);
            task2Completed = true;
            co_return 42;
        }();

        scheduler2.Schedule(task2);

        // Act & Assert
        EXPECT_FALSE(task2Started);

        scheduler2.Run();  // Run task2 up-to awaiting task1

        EXPECT_TRUE(task2Started);
        EXPECT_FALSE(task1Completed);
        EXPECT_FALSE(task2Completed);

        scheduler1.Run();  // Run task1, up-to scheduling task2 continuation on scheduler2

        EXPECT_TRUE(task1Completed);
        EXPECT_FALSE(task2Completed);

        scheduler2.Run();  // Complete task2

        EXPECT_TRUE(task2Completed);
    }

}  // namespace TaskSystem::v1_1::Tests