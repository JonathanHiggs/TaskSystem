#include <TaskSystem/ScheduleItem.hpp>


namespace TaskSystem
{

    ScheduleItem::ScheduleItem(handle_type handle) noexcept : item(handle) { }

    ScheduleItem::ScheduleItem(lambda_type lambda) noexcept : item(lambda) { }

    ScheduleItem::ScheduleItem(function_type function) noexcept : item(function) { }

    std::exception_ptr ScheduleItem::Run() noexcept
    {
        if (auto * handle = std::get_if<handle_type>(&item))
        {
            if (!handle || handle->done())
            {
                // ToDo:
            }
            // Maybe: might want to set the executing scheduler in a way the promise can see it?
            handle->resume();
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