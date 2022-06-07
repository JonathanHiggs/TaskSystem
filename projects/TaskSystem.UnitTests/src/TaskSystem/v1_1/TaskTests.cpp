#include <TaskSystem/Utils/Tracked.hpp>
#include <TaskSystem/v1_1/SynchronousTaskScheduler.hpp>
#include <TaskSystem/v1_1/Task.hpp>

#include <gtest/gtest.h>

#include <thread>


using namespace std::chrono_literals;


namespace TaskSystem::v1_1::Tests
{

    TEST(TaskTests_v1_1, taskLambda)
    {
        // Arrange
        auto started = false;
        auto expected = 42;

        auto task = [&]() -> Task<int> {
            started = true;
            co_return expected;
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    TEST(TaskTests_v1_1, taskLambdaThatThrows)
    {
        // Arrange
        auto started = false;
        auto completed = false;
        auto expected = 42;

        auto task = [&]() -> Task<int> {
            started = true;
            throw std::exception();
            completed = true;
            co_return expected;
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_FALSE(completed);
        EXPECT_EQ(task.State(), TaskState::Error);

        EXPECT_THROW(task.Result(), std::exception);
    }

    TEST(TaskTests_v1_1, taskFrom)
    {
        // Arrange
        auto started = false;
        auto expected = 42;

        auto task = Task<int>::From([&]() {
            started = true;
            return expected;
        });

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    TEST(TaskTests_v1_1, taskFromThatThrows)
    {
        // Arrange
        auto started = false;

        auto task = Task<int>::From([&]() -> int {
            started = true;
            throw std::exception();
        });

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Error);

        EXPECT_THROW(task.Result(), std::exception);
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
        EXPECT_EQ(task.State(), TaskState::Created);
        EXPECT_EQ(innerTask.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);
        EXPECT_EQ(innerTask.State(), TaskState::Created);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.State(), TaskState::Completed);

        EXPECT_EQ(task.Result(), expected);
        EXPECT_EQ(innerTask.Result(), expected);
    }

    TEST(TaskTests_v1_1, taskWithAwaitedTaskThatThrowsPropogatesError)
    {
        // Arrange
        auto started = false;

        auto innerTask = Task<int>::From([&]() -> int {
            started = true;
            throw std::exception();
        });

        auto task = [&]() -> Task<int> {
            auto inner = co_await innerTask;
            co_return inner;
        }();

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);
        EXPECT_EQ(innerTask.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);
        EXPECT_EQ(innerTask.State(), TaskState::Created);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Error);
        EXPECT_EQ(innerTask.State(), TaskState::Error);

        EXPECT_THROW(task.Result(), std::exception);
        EXPECT_THROW(innerTask.Result(), std::exception);
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

    TEST(TaskTests_v1_1, taskScheduledOnDifferentSchedulerSwitchesExecution)
    {
        // Arrange
        auto scheduler1 = SynchronousTaskScheduler();
        auto scheduler2 = SynchronousTaskScheduler();

        auto expected = 42;

        auto innerTaskCompleted = false;
        auto innerTask = Task<int>::From([&]() {
                             EXPECT_EQ(CurrentScheduler(), &scheduler1);
                             innerTaskCompleted = true;
                             return expected;
                         })
                             .ScheduleOn(scheduler1)
                             .ContinueOn(scheduler2);

        auto taskStarted = false;
        auto taskCompleted = false;

        auto task = [&]() -> Task<int> {
            taskStarted = true;
            EXPECT_EQ(CurrentScheduler(), &scheduler2);
            auto value = co_await innerTask;
            EXPECT_EQ(CurrentScheduler(), &scheduler2);
            taskCompleted = true;
            co_return value;
        }();

        scheduler2.Schedule(task);

        // Act & Assert
        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Scheduled);
        EXPECT_FALSE(taskStarted);

        scheduler2.Run();  // Run task up-to awaiting innerTask

        EXPECT_EQ(innerTask.State(), TaskState::Scheduled);
        EXPECT_FALSE(innerTaskCompleted);
        EXPECT_EQ(task.State(), TaskState::Suspended);
        EXPECT_TRUE(taskStarted);
        EXPECT_FALSE(taskCompleted);

        scheduler1.Run();  // Run innerTask, up-to scheduling task continuation on scheduler2

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.Result(), expected);
        EXPECT_EQ(task.State(), TaskState::Suspended);
        EXPECT_TRUE(innerTaskCompleted);
        EXPECT_FALSE(taskCompleted);

        scheduler2.Run();  // Complete task

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.Result(), expected);
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
        EXPECT_TRUE(taskCompleted);
    }

    TEST(TaskTests_v1_1, awaitCompletedTask)
    {
        // Arrange
        auto expected = 42;
        auto scheduler = SynchronousTaskScheduler();
        auto innerTask = [&]() -> Task<int> {
            co_return expected;
        }();
        scheduler.Schedule(innerTask);
        scheduler.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Completed);

        // Act
        auto task = [&]() -> Task<int> {
            co_return co_await innerTask;
        }();

        scheduler.Schedule(task);
        scheduler.Run();

        // Assert
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
    }

    // await a completed task

    TEST(TaskTests_v1_1, waitContinuesOnTaskResult)
    {
        // Arrange
        auto expected = 42;
        auto waiting = false;
        auto completed = false;

        auto scheduler = SynchronousTaskScheduler();
        auto innerTask = [&]() -> Task<int> {
            std::this_thread::sleep_for(2ms);
            co_return expected;
        }();

        // Act
        std::thread waiter([&]() {
            waiting = true;
            innerTask.Wait();
            completed = true;
        });

        std::jthread worker([&]() {
            scheduler.Schedule(innerTask);
            scheduler.Run();
        });

        waiter.join();

        // Assert
        EXPECT_TRUE(waiting);
        EXPECT_TRUE(completed);
    }

    TEST(TaskTests_v1_1, waitContinuesOnTaskException)
    {
        // Arrange
        auto expected = 42;
        auto waiting = false;
        auto completed = false;

        auto scheduler = SynchronousTaskScheduler();
        auto innerTask = [&]() -> Task<int> {
            std::this_thread::sleep_for(2ms);
            throw std::exception();
            co_return expected;
        }();

        // Act
        std::thread waiter([&]() {
            waiting = true;
            innerTask.Wait();
            completed = true;
        });

        std::jthread worker([&]() {
            scheduler.Schedule(innerTask);
            scheduler.Run();
        });

        waiter.join();

        // Assert
        EXPECT_TRUE(waiting);
        EXPECT_TRUE(completed);
    }

    // tracked copy and move number

}  // namespace TaskSystem::v1_1::Tests