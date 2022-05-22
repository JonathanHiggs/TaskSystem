#include <TaskSystem/Utils/TestTaskFactory.hpp>


namespace TaskSystem::Utils
{

    Task<> EmptyTask()
    {
        co_return;
    }

}