#include <TaskSystem/SynchronousTaskScheduler.hpp>
#include <TaskSystem/Task.hpp>
#include <TaskSystem/ValueTask.hpp>
#include <TaskSystem/Utils/Tracked.hpp>

#include <gtest/gtest.h>

using TaskSystem::Utils::Tracked;


namespace TaskSystem::Tests
{

    TEST(ValueTaskTests, hasExpectedResult)
    {
        // Arrange
        auto expected = 42;

        // Act
        auto task = ValueTask<int>(expected);

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    TEST(ValueTaskTests, returnsResultWhenAwaited)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();
        auto valueTask = ValueTask<int>(expected);

        auto outerTask = [&]() -> Task<int> {
            co_return co_await valueTask;
        }();

        scheduler.Schedule(outerTask);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
        EXPECT_EQ(outerTask.Result(), expected);
    }

    TEST(ValueTaskTests, returnsResultWhenRValueAwaited)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();

        auto outerTask = [&]() -> Task<int> {
            co_return co_await ValueTask<int>(expected);
        }();

        scheduler.Schedule(outerTask);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_EQ(outerTask.State(), TaskState::Completed);
        EXPECT_EQ(outerTask.Result(), expected);
    }

    TEST(ValueTaskTests, taskFromResult)
    {
        // Arrange
        auto expected = 42;

        // Act
        auto task = Task<int>::FromResult(expected);

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    TEST(ValueTaskTests, trackedTaskFromResult)
    {
        // Arrange
        auto expected = Tracked();

        // Act
        auto task = Task<Tracked>::FromResult(expected);

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result().Copies(), 1);
        EXPECT_EQ(task.Result().Moves(), 0);
    }

    TEST(ValueTaskTests, trackedTaskFromResultTakeValue)
    {
        // Arrange
        auto expected = Tracked();

        // Act
        auto task = Task<Tracked>::FromResult(expected);
        auto result = task.Result();

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(result.Copies(), 2);
        EXPECT_EQ(result.Moves(), 0);
    }

    TEST(ValueTaskTests, trackedTaskFromResultTakeRef)
    {
        // Arrange
        auto expected = Tracked();

        // Act
        auto task = Task<Tracked>::FromResult(expected);
        auto & result = task.Result();

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(result.Copies(), 1);
        EXPECT_EQ(result.Moves(), 0);
    }

    TEST(ValueTaskTests, trackedTaskFromRValueResult)
    {
        // Act
        auto task = Task<Tracked>::FromResult(Tracked());

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result().Copies(), 0);
        EXPECT_EQ(task.Result().Moves(), 1);
    }

    TEST(ValueTaskTests, trackedConstTaskFromResult)
    {
        // Arrange
        auto const expected = Tracked();

        // Act
        auto task = Task<Tracked>::FromResult(expected);

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result().Copies(), 1);
        EXPECT_EQ(task.Result().Moves(), 0);
    }

}