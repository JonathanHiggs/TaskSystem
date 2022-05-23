#include <TaskSystem/Task.hpp>

#include <TaskSystem/Utils/TestTaskFactory.hpp>
#include <TaskSystem/Utils/Tracked.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>


using TaskSystem::Utils::Tracked;
using TaskSystem::Utils::EmptyTask;
using TaskSystem::Utils::CopyResult;


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

        auto task = Task<int>::From([=]() { return expected; });

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

    TEST(TaskTests, runAfterCompleteThrows)
    {
        // Arrange
        auto task = EmptyTask();
        task.Run();

        // Act & Assert
        EXPECT_THROW(task.Run(), std::exception);
    }

    // Note: this will deadlock - needs a distinction between task initialized and task scheduled
    TEST(TaskTests, DISABLED_resultBeforeCompleteThrows)
    {
        // Arrange
        auto task = CopyResult(42);

        // Act & Assert
        EXPECT_THROW(task.Result(), std::exception);
    }

    TEST(TaskTests, resultAfterCompleteReturnsValue)
    {
        // Arrange
        auto expected = 1;
        auto task = CopyResult(expected);
        task.Run();

        // Act
        auto result = task.Result();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests, waitTaskRunOnThread)
    {
        // Arrange
        auto expected = 42;
        auto result = 0;
        auto task = CopyResult(expected);

        // Act
        std::thread waiter([&]() {
            task.Wait();
            result = task.Result();
        });

        task.Run();
        waiter.join();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests, resultTaskRunOnThread)
    {
        // Arrange
        auto expected = 42;
        auto result = 0;
        auto task = CopyResult(expected);

        // Act
        std::thread waiter([&]() { result = task.Result(); });

        task.Run();
        waiter.join();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests, multipleThreadsWaitTaskRunOnThread)
    {
        // Arrange
        constexpr size_t threadCount = 10u;
        auto returned = 42;
        auto expected = returned * threadCount;
        auto task = CopyResult(returned);
        std::array<std::thread, threadCount> threads;
        std::atomic_int result = 0;

        // Act
        for (auto i = 0u; i < threadCount; ++i)
        {
            threads[i] = std::thread([&]() {
                task.Wait();
                result.fetch_add(task.Result());
            });
        }

        task.Run();

        for (auto i = 0u; i < threadCount; ++i)
        {
            threads[i].join();
        }

        // Assert
        EXPECT_EQ(result.load(), expected);
    }

    TEST(TaskTests, awaitTaskOnThread)
    {
        // Arrange
        auto expected = 42;
        auto result = 0;
        auto task = CopyResult(expected);

        // Act
        std::thread worker([&]() {
            auto workerTask = [&]() -> Task<> { result = co_await task; }();
            workerTask.Run();
        });

        task.Run();
        worker.join();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests, multipleAwaitTaskRunOnThread)
    {
        // Arrange
        constexpr size_t threadCount = 4u;
        auto returned = 42;
        auto expected = returned * threadCount;
        auto task = CopyResult(returned);
        std::array<std::thread, threadCount> threads;
        std::atomic_int result = 0;

        // Act
        for (auto i = 0u; i < threadCount; ++i)
        {
            threads[i] = std::thread([&]() {
                auto threadTask = [&]() -> Task<int> {
                    auto innerResult = co_await task;
                    co_return innerResult;
                }();

                auto threadResult = threadTask.Run();

                result.fetch_add(threadResult);
            });
        }

        task.Run();

        for (auto i = 0u; i < threadCount; ++i)
        {
            threads[i].join();
        }

        // Assert
        EXPECT_EQ(result.load(), expected);
    }

    TEST(TaskTests, awaitTaskAlreadyCompleted)
    {
        // Arrange
        auto expected = 42;

        auto task1 = [&]() -> Task<int> { co_return expected; }();
        task1.Run();

        // Act
        auto task2 = [&]() -> Task<int> {
            auto innerResult = co_await task1;
            co_return innerResult;
        }();

        auto result = task2.Run();

        // Assert
        EXPECT_EQ(result, expected);
    }

}  // namespace TaskSystem::Tests