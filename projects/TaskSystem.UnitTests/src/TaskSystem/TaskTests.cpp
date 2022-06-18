#include <TaskSystem/SynchronousTaskScheduler.hpp>
#include <TaskSystem/Task.hpp>
#include <TaskSystem/Utils/Tracked.hpp>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <thread>


using namespace std::chrono_literals;

using TaskSystem::Utils::Tracked;


namespace TaskSystem::Tests
{

    TEST(TaskTests, taskLambda)
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

    TEST(TaskTests, taskReturnsReference)
    {
        // Arrange
        bool started = false;
        auto expected = 42;

        auto task = [&]() -> Task<int &> {
            started = true;
            co_return std::ref(expected);
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
        EXPECT_EQ(std::addressof(task.Result()), std::addressof(expected));
    }

    TEST(TaskTests, taskReturnsPointer)
    {
        // Arrange
        bool started = false;
        auto expected = 42;

        auto task = [&]() -> Task<int *> {
            started = true;
            co_return &expected;
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
        EXPECT_EQ(task.Result(), &expected);
    }

    TEST(TaskTests, voidTaskLambda)
    {
        // Arrange
        auto started = false;

        auto task = [&]() -> Task<void> {
            started = true;
            co_return;
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
    }

    TEST(TaskTests, taskLambdaThatThrows)
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

    TEST(TaskTests, voidTaskLambdaThatThrows)
    {
        // Arrange
        auto started = false;
        auto completed = false;

        auto task = [&]() -> Task<void> {
            started = true;
            throw std::exception();
            completed = true;
            co_return;
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

        EXPECT_THROW(task.ThrowIfFaulted(), std::exception);
    }

    TEST(TaskTests, taskFrom)
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

    TEST(TaskTests, voidTaskFrom)
    {
        // Arrange
        auto started = false;

        auto task = Task<void>::From([&]() { started = true; });

        auto scheduler = SynchronousTaskScheduler();

        // Act & Assert
        EXPECT_EQ(task.State(), TaskState::Created);

        scheduler.Schedule(task);
        EXPECT_EQ(task.State(), TaskState::Scheduled);

        EXPECT_FALSE(started);

        scheduler.Run();

        EXPECT_TRUE(started);
        EXPECT_EQ(task.State(), TaskState::Completed);
    }

    TEST(TaskTests, taskFromThatThrows)
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

    TEST(TaskTests, awaitedTaskInTask)
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

    TEST(TaskTests, awaitedTaskCreatedInTask)
    {
        // Arrange
        auto started = false;
        auto expected = 42;
        auto innerTaskFn = [&]() -> Task<int> {
            started = true;
            co_return expected;
        };

        auto task = [&]() -> Task<int> {
            auto inner = co_await innerTaskFn();
            EXPECT_EQ(inner, expected);
            co_return inner;
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

    TEST(TaskTests, taskWithAwaitedTaskThatThrowsPropogatesError)
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

    TEST(TaskTests, scheduleTaskTwiceThrows)
    {
        // Arrange
        auto task = Task<int>::From([]() { return 42; });

        auto scheduler = SynchronousTaskScheduler();

        // Act
        scheduler.Schedule(task);

        // Assert
        EXPECT_THROW(scheduler.Schedule(task), std::exception);
    }

    TEST(TaskTests, taskScheduledOnDifferentSchedulerSwitchesExecution)
    {
        // Arrange
        auto scheduler1 = SynchronousTaskScheduler();
        auto scheduler2 = SynchronousTaskScheduler();
        auto scheduler3 = SynchronousTaskScheduler();

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
            EXPECT_EQ(CurrentScheduler(), &scheduler3);
            auto value = co_await innerTask;
            EXPECT_EQ(CurrentScheduler(), &scheduler2);
            taskCompleted = true;
            co_return value;
        }();

        scheduler3.Schedule(task);

        // Act & Assert
        EXPECT_EQ(innerTask.State(), TaskState::Created);
        EXPECT_EQ(task.State(), TaskState::Scheduled);
        EXPECT_FALSE(taskStarted);

        scheduler3.Run();  // Run task up-to awaiting innerTask

        EXPECT_EQ(innerTask.State(), TaskState::Scheduled);
        EXPECT_FALSE(innerTaskCompleted);
        EXPECT_EQ(task.State(), TaskState::Suspended);
        EXPECT_TRUE(taskStarted);
        EXPECT_FALSE(taskCompleted);

        scheduler1.Run();  // Run innerTask, up-to scheduling task continuation on scheduler2

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.Result(), expected);
        EXPECT_EQ(task.State(), TaskState::Scheduled);
        EXPECT_TRUE(innerTaskCompleted);
        EXPECT_FALSE(taskCompleted);

        scheduler2.Run();  // Complete task

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(innerTask.Result(), expected);
        EXPECT_EQ(task.State(), TaskState::Completed);
        EXPECT_EQ(task.Result(), expected);
        EXPECT_TRUE(taskCompleted);
    }

    TEST(TaskTests, awaitCompletedTask)
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

    TEST(TaskTests, waitContinuesOnTaskResult)
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

    TEST(TaskTests, awaitTaskOnThread)
    {
        // Arrange
        auto expected = 42;
        auto result = 0;
        auto scheduler = SynchronousTaskScheduler();

        auto task = [&]() -> Task<int> {
            co_return expected;
        }();
        auto workerTask = [&]() -> Task<int> {
            co_return co_await task;
        }();

        // Act
        std::thread worker([&]() { result = workerTask.Result(); });

        scheduler.Schedule(task);
        scheduler.Schedule(workerTask);
        scheduler.Run();
        worker.join();

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(TaskTests, waitContinuesOnTaskException)
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

    TEST(TaskTests, LValueTestReturnByValueNeverCopied)
    {
        // Arrange
        auto task = []() -> Task<Tracked> {
            co_return Tracked();
        }();

        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(task);

        // Act
        scheduler.Run();
        auto & result = task.Result();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests, LValueReturnByRefNeverCopiedOrMoved)
    {
        // Arrange
        auto tracked = Tracked();
        auto task = [&]() -> Task<Tracked &> {
            co_return tracked;
        }();

        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(task);

        // Act
        scheduler.Run();
        auto & result = task.Result();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_EQ(result.Moves(), 0u);
    }

    TEST(TaskTests, RValueTestReturnByValueNeverCopied)
    {
        // Arrange
        auto task = []() -> Task<Tracked> {
            co_return Tracked();
        }();

        auto scheduler = SynchronousTaskScheduler();
        scheduler.Schedule(task);

        // Act
        scheduler.Run();
        auto & result = task.Result();

        // Assert
        EXPECT_EQ(result.Copies(), 0u);
        EXPECT_GT(result.Moves(), 1u);
    }

    TEST(TaskTests, RValueTestReturnByRefNeverCopiedOrMoved)
    {
        // Arrange
        auto tracked = Tracked();
        auto scheduler = SynchronousTaskScheduler();

        auto task = [&]() -> Task<Tracked &> {
            co_return tracked;
        }();
        scheduler.Schedule(task);

        // Act
        scheduler.Run();

        // Assert
        EXPECT_EQ(tracked.Copies(), 0u);
        EXPECT_EQ(tracked.Moves(), 0u);
    }

    TEST(TaskTests, multipleAwaitingTasks)
    {
        // Arrange
        auto expected = 42;
        auto scheduler1 = SynchronousTaskScheduler();
        auto scheduler2 = SynchronousTaskScheduler();

        auto innerTask = [=]() -> Task<int> {
            co_return expected;
        }()
                                      .ScheduleOn(scheduler2)
                                      .ContinueOn(scheduler1);


        auto taskFn = [&]() -> Task<int> {
            co_return co_await innerTask;
        };

        auto task1 = taskFn();
        auto task2 = taskFn();
        auto task3 = taskFn();
        auto task4 = taskFn();
        auto task5 = taskFn();
        auto task6 = taskFn();

        scheduler1.Schedule(task1);
        scheduler1.Schedule(task2);
        scheduler1.Schedule(task3);
        scheduler1.Schedule(task4);
        scheduler1.Schedule(task5);
        scheduler1.Schedule(task6);

        // Act & Assert
        scheduler1.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Scheduled);
        EXPECT_EQ(task1.State(), TaskState::Suspended);
        EXPECT_EQ(task2.State(), TaskState::Suspended);
        EXPECT_EQ(task3.State(), TaskState::Suspended);
        EXPECT_EQ(task4.State(), TaskState::Suspended);
        EXPECT_EQ(task5.State(), TaskState::Suspended);
        EXPECT_EQ(task6.State(), TaskState::Suspended);

        scheduler2.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Completed);
        EXPECT_EQ(task1.State(), TaskState::Scheduled);
        EXPECT_EQ(task2.State(), TaskState::Scheduled);
        EXPECT_EQ(task3.State(), TaskState::Scheduled);
        EXPECT_EQ(task4.State(), TaskState::Scheduled);
        EXPECT_EQ(task5.State(), TaskState::Scheduled);
        EXPECT_EQ(task6.State(), TaskState::Scheduled);

        scheduler1.Run();

        EXPECT_EQ(innerTask.State(), TaskState::Completed);

        EXPECT_EQ(task1.State(), TaskState::Completed);
        EXPECT_EQ(task1.Result(), expected);

        EXPECT_EQ(task2.State(), TaskState::Completed);
        EXPECT_EQ(task2.Result(), expected);

        EXPECT_EQ(task3.State(), TaskState::Completed);
        EXPECT_EQ(task3.Result(), expected);

        EXPECT_EQ(task4.State(), TaskState::Completed);
        EXPECT_EQ(task4.Result(), expected);

        EXPECT_EQ(task5.State(), TaskState::Completed);
        EXPECT_EQ(task5.Result(), expected);

        EXPECT_EQ(task6.State(), TaskState::Completed);
        EXPECT_EQ(task6.Result(), expected);
    }

}  // namespace TaskSystem::Tests