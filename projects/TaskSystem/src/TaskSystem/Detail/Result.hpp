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

}  // namespace TaskSystem::Detail