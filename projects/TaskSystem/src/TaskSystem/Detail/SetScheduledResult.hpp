#pragma once

#include <TaskSystem/Detail/Enum.hpp>
#include <TaskSystem/Detail/Result.hpp>
#include <TaskSystem/TaskState.hpp>

#include <array>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>


namespace TaskSystem::Detail
{

    class SetScheduledError final
    {
    public:
        enum ValueType
        {
            CannotSchedule,
            AlreadyScheduled,
            PromiseRunning,
            PromiseCompleted,
            PromiseFaulted
        };

    private:
        ValueType value;

    public:
        constexpr SetScheduledError(ValueType value) noexcept : value(value) { }

        [[nodiscard]] constexpr operator ValueType() const noexcept { return value; }

        [[nodiscard]] constexpr std::string_view ToStringView() const noexcept
        {
            switch (value)
            {
            // clang-format off
            case CannotSchedule:    return "CannotSchedule";
            case AlreadyScheduled:  return "AlreadyScheduled";
            case PromiseRunning:    return "PromiseRunning";
            case PromiseCompleted:  return "PromiseCompleted";
            case PromiseFaulted:    return "PromiseFaulted";
            default:                return "Unknown";
            // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const noexcept { return std::string(ToStringView()); }
    };

    static constexpr std::array<SetScheduledError, 0u> SetScheduledErrors{};

    inline std::ostream & operator<<(std::ostream & os, SetScheduledError const value)
    {
        return os << value.ToStringView();
    }

    using SetScheduledResult = Result<SetScheduledError>;

}  // namespace TaskSystem::Detail