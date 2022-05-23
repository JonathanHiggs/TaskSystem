#include <TaskSystem/v1_0/Task.hpp>

#include <TaskSystem/Utils/TestTaskFactory.hpp>
#include <TaskSystem/Utils/Tracked.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>


using TaskSystem::Utils::Tracked;
using TaskSystem::Utils::EmptyTask;
using TaskSystem::Utils::CopyResult;


namespace TaskSystem::v1_0::Tests
{

    TEST(TaskTests_v1_0, taskFromLambdaReturnVoid)
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

    TEST(TaskTests_v1_0, taskFromLambdaReturnValue)
    {
        // Arrange
        auto expected = 42;

        auto task = Task<int>::From([=]() { return expected; });

        // Act
        auto result = task.Run();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests_v1_0, taskLambdaReturnVoid)
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

    TEST(TaskTests_v1_0, taskLambdaReturnValue)
    {
        // Arrange
        auto expected = 42;

        auto task = [=]() -> Task<int> { co_return expected; }();

        // Act
        auto result = task.Run();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests_v1_0, taskStartsWhenRun)
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

    TEST(TaskTests_v1_0, taskStartsWhenAwaited)
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

    TEST(TaskTests_v1_0, runDefaultConstructedTaskThrows)
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

    TEST(TaskTests_v1_0, taskRuturnsValue)
    {
        // Arrange
        auto expected = 42;
        auto task = taskFn(expected);

        // Act
        auto actual = task.Run();

        // Assert
        EXPECT_EQ(actual, expected);
    }

    TEST(TaskTests_v1_0, taskReturnsReference)
    {
        // Arrange
        auto expected = 42;
        auto task = [&]() -> Task<int &> { co_return expected; }();

        // Act
        auto & actual = task.Run();

        // Assert
        EXPECT_EQ(actual, expected);
    }

    TEST(TaskTests_v1_0, taskReturnsPointer)
    {
        // Arrange
        auto expected = 42;
        auto task = [&]() -> Task<int *> { co_return &expected; }();

        // Act
        auto * actual = task.Run();

        // Assert
        EXPECT_EQ(actual, &expected);
    }

    TEST(TaskTests_v1_0, taskNotReadyBeforeRun)
    {
        // Arrange
        auto task = []() -> Task<> { co_return; }();

        // Act
        auto isReady = task.IsReady();

        // Assert
        EXPECT_FALSE(isReady);
    }

    TEST(TaskTests_v1_0, taskReadyAfterRun)
    {
        // Arrange
        auto task = []() -> Task<> { co_return; }();
        task.Run();

        // Act
        auto isReady = task.IsReady();

        // Assert
        EXPECT_TRUE(isReady);
    }

    TEST(TaskTests_v1_0, LValueTestReturnByValueCopiedOnce)
    {
        // Arrange
        auto task = []() -> Task<Tracked> { co_return Tracked(); }();

        // Act
        auto result = task.Run();

        // Assert
        EXPECT_EQ(result.Copies(), 1u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests_v1_0, LValueReturnByRefNeverCopiedOrMoved)
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

    TEST(TaskTests_v1_0, RValueTestReturnByValueNeverCopied)
    {
        // Arrange
        auto task = []() -> Task<Tracked> { co_return Tracked(); };

        // Act
        auto result = task().Run();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests_v1_0, RValueTestReturnByRefNeverCopiedOrMoved)
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

    TEST(TaskTests_v1_0, runAfterCompleteThrows)
    {
        // Arrange
        auto task = EmptyTask();
        task.Run();

        // Act & Assert
        EXPECT_THROW(task.Run(), std::exception);
    }

    // Note: this will deadlock - needs a distinction between task initialized and task scheduled
    TEST(TaskTests_v1_0, DISABLED_resultBeforeCompleteThrows)
    {
        // Arrange
        auto task = CopyResult(42);

        // Act & Assert
        EXPECT_THROW(task.Result(), std::exception);
    }

    TEST(TaskTests_v1_0, resultAfterCompleteReturnsValue)
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

    TEST(TaskTests_v1_0, waitTaskRunOnThread)
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

    TEST(TaskTests_v1_0, resultTaskRunOnThread)
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

    TEST(TaskTests_v1_0, multipleThreadsWaitTaskRunOnThread)
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

    TEST(TaskTests_v1_0, awaitTaskOnThread)
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

    TEST(TaskTests_v1_0, multipleAwaitTaskRunOnThread)
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

    TEST(TaskTests_v1_0, awaitTaskAlreadyCompleted)
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