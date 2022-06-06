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
        enum ValueType // : u8
        {
            Created,
            Scheduled,
            Running,
            Suspended,
            Completed,
            Cancelled, // ToDo: remove - not used
            Error,     // ToDo: rename Faulted
            Unknown
        };

    private:
        ValueType value;

    public:
        constexpr TaskState(ValueType value) : value(value)
        { }

        [[nodiscard]] constexpr operator ValueType() const noexcept
        {
            return value;
        }

        [[nodiscard]] constexpr bool IsCompleted() const noexcept
        {
            switch (value)
            {
            case Completed:
            case Cancelled:
            case Error: return true;

            default: return false;
            }
        }

        [[nodiscard]] constexpr std::string_view ToStringView() const noexcept
        {
            switch (value)
            {
            // clang-format off
            case Created:   return "Created";
            case Scheduled: return "Scheduled";
            case Running:   return "Running";
            case Suspended: return "Suspended";
            case Completed: return "Completed";
            case Cancelled: return "Cancelled";
            case Error:     return "Error";
            case Unknown:
            default:        return "Unknown";
                // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const noexcept
        {
            return std::string(ToStringView());
        }
    };

    static constexpr std::array<TaskState, 8u> TaskStates{ TaskState::Created,   TaskState::Scheduled,
                                                           TaskState::Running,   TaskState::Suspended,
                                                           TaskState::Completed, TaskState::Cancelled,
                                                           TaskState::Error,     TaskState::Unknown };

    std::ostream & operator<<(std::ostream & os, TaskState const value);

}  // namespace TaskSystem