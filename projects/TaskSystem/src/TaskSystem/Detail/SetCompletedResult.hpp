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

    class SetCompletedError final
    {
    public:
        enum ValueType
        {
            AlreadyCompleted,
            PromiseScheduled,
            PromiseFaulted,
        };

    private:
        ValueType value;

    public:
        constexpr SetCompletedError(ValueType value) noexcept : value(value) { }

        operator bool() const noexcept = delete;

        [[nodiscard]] constexpr operator ValueType() const noexcept { return value; }

        [[nodiscard]] constexpr std::string_view ToStringView() const noexcept
        {
            switch (value)
            {
            // clang-format off
            case AlreadyCompleted: return "AlreadyCompleted";
            case PromiseScheduled: return "PromiseScheduled";
            case PromiseFaulted:   return "PromiseFaulted";
            default:               return "Unknown";
            // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const noexcept { return std::string(ToStringView()); }
    };

    static constexpr std::array<SetCompletedError, 0u> SetCompletedErrors{};

    inline std::ostream & operator<<(std::ostream & os, SetCompletedError const value)
    {
        return os << value.ToStringView();
    }

    using SetCompletedResult = Result<SetCompletedError>;

}  // namespace TaskSystem::Detail