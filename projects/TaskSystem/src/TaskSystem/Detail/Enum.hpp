#pragma once

#include <type_traits>


namespace TaskSystem::Detail
{

    template <typename T, typename = void>
    inline constexpr bool IsEnum = false;

    template <typename T>
    inline constexpr bool IsEnum<T, std::void_t<decltype(std::declval<T::ValueType>())>> = true;

    template <typename T>
    concept Enum = IsEnum<T>;

}  // namespace TaskSystem::Detail