#pragma once

#include <array>
#include <ostream>
#include <string>
#include <string_view>


namespace TaskSystem
{

    class TaskState final
    {
    public:
        enum ValueType  // : u8
        {
            Created,
            Scheduled,
            Running,
            Completed,
            Canceled,
            Error,  // ToDo: rename Faulted
            Unknown
        };

    private:
        ValueType value;

    public:
        constexpr TaskState(ValueType value) : value(value)
        { }

        [[nodiscard]] constexpr operator ValueType() const
        {
            return value;
        }

        [[nodiscard]] constexpr std::string_view ToStringView() const
        {
            switch (value)
            {
            // clang-format off
            case Created:   return "Created";
            case Scheduled: return "Scheduled";
            case Running:   return "Running";
            case Completed: return "Completed";
            case Canceled:  return "Canceled";
            case Error:     return "Error";
            case Unknown:
            default:        return "Unknown";
                // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const
        {
            return std::string(ToStringView());
        }
    };

    static constexpr std::array<TaskState, 7u> TaskStates{ TaskState::Created,  TaskState::Scheduled,
                                                           TaskState::Running,  TaskState::Completed,
                                                           TaskState::Canceled, TaskState::Error,
                                                           TaskState::Unknown };

    std::ostream & operator<<(std::ostream & os, TaskState const value);

}  // namespace TaskSystem