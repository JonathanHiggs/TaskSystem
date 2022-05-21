#include <TaskSystem/Task.hpp>

#include <TaskSystem/Utils/Tracked.hpp>

#include <gtest/gtest.h>


using TaskSystem::Utils::Tracked;


namespace TaskSystem::Tests
{

    TEST(TaskTests, taskFromLambdaReturnVoid)
    {
        // Arrange
        bool started = false;

        auto task = Task<>::From([&]() { started = true; });

        EXPECT_FALSE(started);

        // Act
        task.Run();

        // Assert
        EXPECT_TRUE(started);
    }

    TEST(TaskTests, taskFromLambdaReturnValue)
    {
        // Arrange
        auto expected = 42;

        auto task = Task<>::From([=]() { return expected; });

        // Act
        auto result = task.Run();

        // Assert
        EXPECT_EQ(result, expected);
    }

     TEST(TaskTests, taskLambdaReturnVoid)
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

     TEST(TaskTests, taskLambdaReturnValue)
     {
         // Arrange
         auto expected = 42;

         auto task = [=]() -> Task<int> { co_return expected; }();

         // Act
         auto result = task.Run();

         // Assert
         EXPECT_EQ(result, expected);
     }

    TEST(TaskTests, taskStartsWhenRun)
    {
        // Arrange
        bool started = false;

        auto task = Task<>::From([&]() { started = true; });

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

    Task<int> taskFn(int expected)
    {
        co_return expected;
    };

    TEST(TaskTests, taskRuturnsValue)
    {
        // Arrange
        auto expected = 42;
        auto task = taskFn(expected);

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

    TEST(TaskTests, LValueTestReturnByValueCopiedOnce)
    {
        // Arrange
        auto task = []() -> Task<Tracked> { co_return Tracked(); }();

        // Act
        auto result = task.Run();

        // Assert
        EXPECT_EQ(result.Copies(), 1u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests, LValueReturnByRefNeverCopiedOrMoved)
    {
        // Arrange
        auto tracked = Tracked();
        auto task = [&]() -> Task<Tracked &> { co_return tracked; }();

        // Act
        auto & result = task.Run();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_EQ(result.Moves(), 0u);
    }

    TEST(TaskTests, RValueTestReturnByValueNeverCopied)
    {
        // Arrange
        auto task = []() -> Task<Tracked> { co_return Tracked(); };

        // Act
        auto result = task().Run();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests, RValueTestReturnByRefNeverCopiedOrMoved)
    {
        // Arrange
        auto tracked = Tracked();
        auto task = [&]() -> Task<Tracked &> { co_return tracked; };

        // Act
        auto & result = task().Run();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_EQ(result.Moves(), 0u);
    }

    /* ToDo:
     *   Run after completed
     *   Tasks throw exceptions
     *   Multiple tasks await result
     */

}  // namespace TaskSystem::Tests