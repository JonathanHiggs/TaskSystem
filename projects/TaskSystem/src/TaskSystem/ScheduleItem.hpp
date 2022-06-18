#pragma once

#include <coroutine>
#include <exception>
#include <functional>
#include <variant>


namespace TaskSystem
{
    namespace Detail
    {
        class IPromise;
    }

    class ScheduleItem
    {
    private:
        using promise_type = Detail::IPromise;
        using promise_type_ptr = Detail::IPromise *;
        using lambda_type = std::function<void()>;
        using function_type = void (*)();

        std::variant<promise_type_ptr, lambda_type, function_type> item;

    public:
        ScheduleItem(promise_type & promise) noexcept;
        ScheduleItem(promise_type_ptr promise) noexcept;

        ScheduleItem(lambda_type lambda) noexcept;
        ScheduleItem(function_type function) noexcept;

        std::exception_ptr Run() noexcept;
    };

}  // namespace TaskSystem