#pragma once


namespace TaskSystem::v1_1::Detail
{

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

}  // namespace TaskSystem::v1_1::Detail