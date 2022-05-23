#pragma once

#include <coroutine>
#include <exception>
#include <functional>
#include <variant>


namespace TaskSystem::v1_1
{

    class ScheduleItem
    {
    private:
        using handle_type = std::coroutine_handle<>;
        using lambda_type = std::function<void()>;
        using function_type = void (*)();

        std::variant<handle_type, lambda_type, function_type> item;

    public:
        ScheduleItem(handle_type handle) noexcept;
        ScheduleItem(lambda_type lambda) noexcept;
        ScheduleItem(function_type function) noexcept;

        std::exception_ptr Run() noexcept;
    };

    // template <typename = void>
    // class ScheduleItem;
    //
    // template <>
    // class ScheduleItem<void>
    // {
    // private:
    //     std::function<void()> func;
    //
    // public:
    //     ScheduleItem(std::function<void()> && func) : func(std::move(func))
    //     { }
    //
    //     std::exception_ptr Run() noexcept;
    // };
    //
    // template<>
    // class ScheduleItem<std::function<void()>
    // {
    //
    // };
    //
    // template <>
    // class ScheduleItem<void (*)()>
    // {
    //
    // };
    //
    // template<typename TResult>
    // class ScheduleItem<Task<TResult>>
    // {
    //
    // };

}  // namespace TaskSystem::v1_1