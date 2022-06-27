# TaskSystem

A experimental c++20 coroutines-based asynchronous task library 

[cppcoro](https://github.com/lewissbaker/cppcoro) is a fantastic start at creating a coroutine based task system, but
development has not progressed for a few years and it is missing some features that would be extremely
useful for a full production system

Currently the code is in very early stages and is only tested on Windows + Visual Studio 2022

## Features

### Task

Functions that return the `Task<>` type create a promise and `std::coroutine_handle<>` that is schedulable and
awaitable

```cpp
#include <TaskSystem/Task.hpp>
#include <TaskSystem/SynchronousTaskScheduler.hpp>

#include <iostream>

TaskSystem::Task<int> CalculateAsync()
{
    co_return 42;
}

TaskSystem::Task<> RunCalculationAsync()
{
    auto result = co_await CalculateAsync();
    std::cout << result << '\n';
}

int main()
{
    auto task = RunCalculationsAsync();
    std::cout << task.State().ToString() << '\n'; // Created;

    auto scheduler = TaskSystem::SynchronousTaskScheduler();
    scheduler.Schedule(task);
    std::cout << task.State().ToString() << '\n'; // Scheduled;

    scheduler.Run();
    std::cout << task.State().ToString() << '\n'; // Completed;
}
```

Note: `SynchronousTaskScheduler` is a special scheduler that runs all tasks on the main thread when the `Run`
function is invoked; only useful for controlling execution of tasks for demonstration and testing

Tasks can also be defined inline as lambdas and immediately invoked lambdas. The trailing return type is needed
otherwise the compile will not know to apply the coroutine mechanism

```cpp
auto taskFn = []() -> Task<int> { co_return 42; }
auto task1 = taskFn();

auto task2 = []() -> Task<int> { co_return 42; }();
```

Continuations can be scheduled across different schedulers, eg. a UI framework can run some work on one Scheduler
(global thread pool) and schedule the a continuation that updates the UI resources on the UI scheduler to avoid locks
and contention

```cpp
auto scheduler1 = SynchronousTaskScheduler();
auto scheduler2 = SynchronousTaskScheduler();

auto innerTask = []() -> Task<int> { co_return 42; }();

innerTask.ScheduleOn(scheduler1).ContinueOn(scheduler2);

auto outerTask = [&]() -> Task<int> { co_return co_await innerTask; }();

scheduler2.Schedule(outerTask);
scheduler2.Run();

std::cout << innerTask.State().ToString() << '\n'; // Scheduled;
std::cout << outerTask.State().ToString() << '\n'; // Suspended;

scheduler1.Run();

std::cout << innerTask.State().ToString() << '\n'; // Completed;
std::cout << outerTask.State().ToString() << '\n'; // Scheduled;

scheduler2.Run();

std::cout << innerTask.State().ToString() << '\n'; // Completed;
std::cout << outerTask.State().ToString() << '\n'; // Completed;
```


### ValueTask

`ValueTask` is an awaitable type that does not have the overhead of a promise and coroutine frame, used when a result
is already known. e.g. when a result is loaded async initially and cached for future calls, or when a synchronous
method must fulfil a async contract 

```cpp
auto completedTask = ValueTask<int>(42);
std::cout << completedTask.State().ToString() << '\n'; // Completed;

auto task = [&]() -> Task<int> { co_return co_await completedTask; }();

auto scheduler = SynchronousTaskScheduler();
scheduler.Schedule(task);
scheduler.Run();

std::cout << task.Result() << '\n'; // 42
```

### TaskCompletionSource

`TaskCompletionSource` allows manual control over when a task completes by setting a result (or setting an exception).
The use-case for this is to allow a system (eg. sockets or messaging) to send out network calls, and return a task 
immediately. The system can hold onto the `TaskCompletionSource` and complete the task when a response comes back

```cpp
auto taskCompletionSource = TaskCompletionSource<int>();

auto task = [&]() -> Task<int> {
    co_return co_await taskCompletionSource.Task();
}();

auto scheduler = SynchronousTaskScheduler();
scheduler.Schedule(task);
scheduler.Run();

std::cout << task.State().ToString() << '\n'; // Suspended;

taskCompletionSource.SetResult(42);

std::cout << task.State().ToString() << '\n'; // Scheduled;

scheduler.Run();

std::cout << task.State().ToString() << '\n'; // Completed;
```

### WhenAll

`WhenAll` can be used to wait for multiple non-sequential tasks to complete simultaneously 

```cpp
auto taskCompletionSource1 = TaskCompletionSource<int>();
auto task1 = taskCompletionSource1.Task();

auto taskCompletionSource2 = TaskCompletionSource<int>();
auto task2 = taskCompletionSource1.Task();

auto whenAllTask = [&]() -> Task<> { co_await WhenAll(task1, task2); }();

auto scheduler = SynchronousTaskScheduler();
scheduler.Schedule(whenAllTask);
scheduler.Run();

std::cout << whenAllTask.State().ToString() << '\n'; // Suspended;

taskCompletionSource1.SetResult(1);s

std::cout << whenAllTask.State().ToString() << '\n'; // Suspended;

taskCompletionSource1.SetResult(2);

std::cout << whenAllTask.State().ToString() << '\n'; // Scheduled;

scheduler.Run();
std::cout << whenAllTask.State().ToString() << '\n'; // Completed;

std::cout << task1.Result() + task2.Result() << '\n'; // 3
```

### WhenAny (WIP)

`WhenAny` can be used to wait for the first of multiple tasks to complete simultaneously, e.g. query the US and EU
servers and use the first result for minimal latentcy
