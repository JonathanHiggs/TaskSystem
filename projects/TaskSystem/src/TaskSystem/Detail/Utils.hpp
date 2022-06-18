#pragma once

#include <atomic>


namespace TaskSystem::Detail
{

    inline constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;

    // ToDo: there is almost certainly a better way of doing this
    template <typename T>
    inline T * FirstOf(T * value1, T * value2)
    {
        if (value1)
        {
            return value1;
        }

        return value2;
    }

    template <typename T>
    inline T * FirstOf(T * value1, T * value2, T * value3)
    {
        auto * result = FirstOf(value1, value2);
        if (result)
        {
            return result;
        }

        return value3;
    }

    template <typename T>
    inline T * FirstOf(T * value1, T * value2, T * value3, T * value4)
    {
        auto * result = FirstOf(value1, value2, value3);
        if (result)
        {
            return result;
        }

        return value4;
    }

}  // namespace TaskSystem::Detail