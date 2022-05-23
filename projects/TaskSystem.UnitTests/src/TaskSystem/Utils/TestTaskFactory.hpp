#pragma once

#include <TaskSystem/v1_0/Task.hpp>

#include <type_traits>


namespace TaskSystem::Utils
{

    Task<> EmptyTask();

    template <typename T>
    Task<T> CopyResult(T const & value)
    {
        co_return value;
    }

    template <typename TFunc, typename TResult = std::invoke_result_t<TFunc>>
    Task<TResult> FromLambda(TFunc && func)
    {
        if constexpr (std::is_void_v<TResult>)
        {
            func();
            co_return;
        }
        else
        {
            co_return func();
        }
    }

}  // namespace TaskSystem::Utils