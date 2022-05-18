#include <TaskSystem/Task.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Tests
{

    TEST(TaskTests, taskStartsWhenRun)
    {
        // Arrange
        bool started = false;

        auto task = [&]() -> Task<> {
            started = true;
            co_return;
        }();

        EXPECT_FALSE(started);

        // Act
        task.Run();

        // Assert
        EXPECT_TRUE(started);
    }

    TEST(TaskTests, taskStartsWhenAwaited)
    {
        // Arrange
        bool started = false;

        auto fn = [&]() -> Task<> {
            started = true;
            co_return;
        };

        auto task = [&]() -> Task<> {
            EXPECT_FALSE(started);
            co_await fn();
        }();

        // Act
        task.Run();

        // Assert
        EXPECT_TRUE(started);
    }

    TEST(TaskTests, runDefaultConstructedTaskThrows)
    {
        // Arrange
        auto task = Task<>();

        // Act & Assert
        EXPECT_THROW(task.Run(), std::exception);
    }

    TEST(TaskTests, taskRuturnsValue)
    {
        // Arrange
        auto expected = 42;
        auto task = [=]() -> Task<int> { co_return expected; }();

        // Act
        auto actual = task.Run();

        // Assert
        EXPECT_EQ(actual, expected);
    }

    TEST(TaskTests, taskReturnsReference)
    {
        // Arrange
        auto expected = 42;
        auto task = [&]() -> Task<int &> { co_return expected; }();

        // Act
        auto & actual = task.Run();

        // Assert
        EXPECT_EQ(actual, expected);
    }

    TEST(TaskTests, taskReturnsPointer)
    {
        // Arrange
        auto expected = 42;
        auto task = [&]() -> Task<int *> { co_return &expected; }();

        // Act
        auto * actual = task.Run();

        // Assert
        EXPECT_EQ(actual, &expected);
    }

    TEST(TaskTests, taskNotReadyBeforeRun)
    {
        // Arrange
        auto task = []() -> Task<> { co_return; }();

        // Act
        auto isReady = task.IsReady();

        // Assert
        EXPECT_FALSE(isReady);
    }

    TEST(TaskTests, taskReadyAfterRun)
    {
        // Arrange
        auto task = []() -> Task<> { co_return; }();
        task.Run();

        // Act
        auto isReady = task.IsReady();

        // Assert
        EXPECT_TRUE(isReady);
    }

}  // namespace TaskSystem::Tests