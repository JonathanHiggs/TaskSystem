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
        auto task = Task<int>::From([]() {
            return 42;
        });

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);

        // Assert
        EXPECT_THROW(scheduler.Schedule(task), std::exception);
    }

}  // namespace TaskSystem::v1_1::Tests