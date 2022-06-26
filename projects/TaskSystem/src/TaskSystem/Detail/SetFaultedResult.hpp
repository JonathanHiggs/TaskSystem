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

    class SetFaultedError final
    {
    public:
        enum ValueType
        {
        };

    private:
        ValueType value;

    public:
        constexpr SetFaultedError(ValueType value) noexcept : value(value) { }

        [[nodiscard]] constexpr operator ValueType() const noexcept { return value; }

        [[nodiscard]] constexpr std::string_view ToStringView() const noexcept
        {
            switch (value)
            {
            // clang-format off
            // clang-format on
            }
        }

        [[nodiscard]] constexpr std::string ToString() const noexcept { return std::string(ToStringView()); }
    };

    static constexpr std::array<SetFaultedError, 0u> SetFaultedErrors{};

    inline std::ostream & operator<<(std::ostream & os, SetFaultedError const value)
    {
        return os << value.ToStringView();
    }

    using SetFaultedResult = Result<SetFaultedError>;

}  // namespace TaskSystem::Detail