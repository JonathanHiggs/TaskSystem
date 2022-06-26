#pragma once

#include <TaskSystem/Detail/Enum.hpp>

#include <optional>


namespace TaskSystem::Detail
{

    struct SuccessTag
    {
    };

    constexpr inline static SuccessTag Success{};

    template <typename TErrorReason>
    class Result
    {
    private:
        std::optional<TErrorReason> reason;

    public:
        constexpr Result() noexcept : reason(std::nullopt) { }

        constexpr Result(SuccessTag) noexcept : reason(std::nullopt) { }

        template <typename TAlias = TErrorReason>
        constexpr Result(TAlias && reason) noexcept : reason(std::forward<TAlias>(reason))
        { }

        template <typename TAlias = TErrorReason, std::enable_if_t<IsEnum<TErrorReason>> * = nullptr>
        constexpr Result(TErrorReason::ValueType reason) noexcept : reason(TErrorReason(reason))
        { }

        [[nodiscard]] operator bool() const noexcept { return !reason; }

        [[nodiscard]] TErrorReason operator*() const noexcept { return *reason; }
    };


    template <typename TErrorReason>
    constexpr bool operator==(Result<TErrorReason> const & lhs, bool rhs)
    {
        return static_cast<bool>(lhs) == rhs;
    }

    template <typename TErrorReason>
    [[nodiscard]] constexpr inline bool operator!=(Result<TErrorReason> const & lhs, bool rhs)
    {
        return !(lhs == rhs);
    }

    template <typename TErrorReason>
    constexpr bool operator==(bool lhs, Result<TErrorReason> const & rhs)
    {
        return rhs == lhs;
    }

    template <typename TErrorReason>
    [[nodiscard]] constexpr inline bool operator!=(bool lhs, Result<TErrorReason> const & rhs)
    {
        return !(lhs == rhs);
    }


    template <typename TErrorReason>
    constexpr bool operator==(Result<TErrorReason> const & lhs, TErrorReason const & rhs)
    {
        return !lhs && *lhs == rhs;
    }

    template <typename TErrorReason>
    [[nodiscard]] constexpr inline bool operator!=(Result<TErrorReason> const & lhs, TErrorReason const & rhs)
    {
        return !(lhs == rhs);
    }

    template <typename TErrorReason>
    constexpr bool operator==(TErrorReason const & lhs, Result<TErrorReason> const & rhs)
    {
        return rhs == lhs;
    }

    template <typename TErrorReason>
    [[nodiscard]] constexpr inline bool operator!=(TErrorReason const & lhs, Result<TErrorReason> const & rhs)
    {
        return !(lhs == rhs);
    }


    template <
        typename TErrorReason,
        typename TValueType = TErrorReason::ValueType,
        std::enable_if_t<IsEnum<TErrorReason>> * = nullptr>
    constexpr bool operator==(Result<TErrorReason> const & lhs, TValueType rhs)
    {
        return !lhs && *lhs == TErrorReason(rhs);
    }

    template <
        typename TErrorReason,
        typename TValueType = TErrorReason::ValueType,
        std::enable_if_t<IsEnum<TErrorReason>> * = nullptr>
    [[nodiscard]] constexpr inline bool operator!=(Result<TErrorReason> const & lhs, TValueType rhs)
    {
        return !(lhs == rhs);
    }

    template <
        typename TErrorReason,
        typename TValueType = TErrorReason::ValueType,
        std::enable_if_t<IsEnum<TErrorReason>> * = nullptr>
    constexpr bool operator==(TValueType lhs, Result<TErrorReason> const & rhs)
    {
        return rhs == lhs;
    }

    template <
        typename TErrorReason,
        typename TValueType = TErrorReason::ValueType,
        std::enable_if_t<IsEnum<TErrorReason>> * = nullptr>
    [[nodiscard]] constexpr inline bool operator!=(TValueType lhs, Result<TErrorReason> const & rhs)
    {
        return !(lhs == rhs);
    }

}  // namespace TaskSystem::Detail