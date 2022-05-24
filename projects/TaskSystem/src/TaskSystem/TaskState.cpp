#include <TaskSystem/TaskState.hpp>


namespace TaskSystem
{

    std::ostream & operator<<(std::ostream & os, TaskState const value)
    {
        return os << value.ToStringView();
    }

}  // namespace TaskSystem