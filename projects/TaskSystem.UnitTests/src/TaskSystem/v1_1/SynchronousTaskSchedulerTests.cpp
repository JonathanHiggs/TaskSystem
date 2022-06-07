#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>
#include <TaskSystem/v1_1/Task.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{

    TEST(SynchronousTaskSchedulerTests_v1_1, runWithLambdas)
    {
        // Arrange
        auto completed1 = false;
        auto completed2 = false;

        auto task1 = [&]() {
            completed1 = true;
        };
        auto task2 = [&]() {
            completed2 = true;
        };

        auto scheduler = SynchronousTaskScheduler();

        scheduler.Schedule(ScheduleItem(std::move(task1)));
        scheduler.Schedule(ScheduleItem(std::move(task2)));

        // Act
        scheduler.Run();

        // Assert
        EXPECT_TRUE(completed1);
        EXPECT_TRUE(completed2);
    }

    static bool completed = false;

    void SetCompleted()
    {
        completed = true;
    }

    TEST(SynchronousTaskSchedulerTests_v1_1, runWithFunctionPointer)
    {
        // Arrange
        auto scheduler = SynchronousTaskScheduler();

        scheduler.Schedule(ScheduleItem(SetCompleted));

        // Act
        scheduler.Run();

        // Assert
        EXPECT_TRUE(completed);
    }

    TEST(SynchronousTaskSchedulerTests_v1_1, isWorkingThread)
    {
        // Arrange
        auto isWorkerThread = false;
        auto scheduler = SynchronousTaskScheduler();
        auto task = [&]() -> Task<int> {
            isWorkerThread = scheduler.IsWorkerThread();
            co_return 42;
        }();
        scheduler.Schedule(task);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_FALSE(scheduler.IsWorkerThread());
        EXPECT_TRUE(isWorkerThread);
    }

}  // namespace TaskSystem::v1_1::Tests