#pragma once

#include <TaskSystem/TaskState.hpp>

#include <tl/expected.hpp>

#include <string>
#include <variant>


namespace TaskSystem::Detail
{

    // Maybe: not in detail?

    class AddContinuationError final
    {
    public:
        enum ValueType
        {
            InvalidContinuation,
            PromiseCompleted,
            PromiseFaulted
        };

    private:
        ValueType value;

    public:
        constexpr AddContinuationError(ValueType value) noexcept : value(value) { }

        [[nodiscard]] constexpr operator ValueType() const noexcept { return value; }

        [[nodiscard]] constexpr std::string_view ToStringView() const noexcept
        {
            switch (value)
            {
            // clang-format off
            case InvalidContinuation: return "InvalidContinuation";
            case PromiseCompleted:    return "PromiseCompleted";
            case PromiseFaulted:      return "PromiseFaulted";
            default:                  return "Unknown";
            // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const noexcept
        {
            return std::string(ToStringView());
        }
    };

    using AddContinuationResult = tl::expected<std::monostate, AddContinuationError>;

}