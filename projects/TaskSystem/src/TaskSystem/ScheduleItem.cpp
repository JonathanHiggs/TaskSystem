#include <TaskSystem/Detail/IPromise.hpp>
#include <TaskSystem/ScheduleItem.hpp>


namespace TaskSystem
{

    ScheduleItem::ScheduleItem(promise_type & promise) noexcept : item(&promise) { }

    ScheduleItem::ScheduleItem(promise_type_ptr promise) noexcept : item(promise) { }

    ScheduleItem::ScheduleItem(lambda_type lambda) noexcept : item(lambda) { }

    ScheduleItem::ScheduleItem(function_type function) noexcept : item(function) { }

    std::exception_ptr ScheduleItem::Run() noexcept
    {
        if (auto * ppromise = std::get_if<promise_type_ptr>(&item))
        {
            if (!(*ppromise))
            {
                // ToDo:
                return nullptr;
            }

            promise_type & promise = **ppromise;

            if (!promise.TrySetRunning())
            {
                // ToDo:
                return nullptr;
            }

            auto handle = promise.Handle();
            if (!handle || handle.done())
            {
                // ToDo:
                return nullptr;
            }

            handle.resume();
        }
        else if (auto * lambda = std::get_if<lambda_type>(&item))
        {
            try
            {
                (*lambda)();
            }
            catch (...)
            {
                return std::current_exception();
            }
        }
        else if (auto * function = std::get_if<function_type>(&item))
        {
            try
            {
                (*function)();
            }
            catch (...)
            {
                return std::current_exception();
            }
        }

        return nullptr;
    }

}  // namespace TaskSystem