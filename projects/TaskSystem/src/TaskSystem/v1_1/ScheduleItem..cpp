#include <TaskSystem/v1_1/ScheduleItem.hpp>


namespace TaskSystem::v1_1
{

    ScheduleItem::ScheduleItem(handle_type handle) noexcept : item(handle)
    { }

    ScheduleItem::ScheduleItem(lambda_type lambda) noexcept : item(lambda)
    { }

    ScheduleItem::ScheduleItem(function_type function) noexcept : item(function)
    { }

    std::exception_ptr ScheduleItem::Run() noexcept
    {
        if (auto * handle = std::get_if<handle_type>(&item))
        {
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

}  // namespace TaskSystem::v1_1