#pragma once

#include <exception>
#include <variant>


namespace TaskSystem::v1_1::Detail
{

    struct Created final : std::monostate
    {
    };

    struct Scheduled final : std::monostate
    {
    };

    struct Running final : std::monostate
    {
    };

    struct Suspended final : std::monostate
    {
    };

    template <typename TResult>
    struct Completed final
    {
        TResult Value;
    };

    template <>
    struct Completed<void> final : std::monostate
    {
    };

    // Maybe: CompletedResultMoved

    // Maybe: Cancelled?

    struct Faulted final
    {
        std::exception_ptr Exception;
    };

}  // namespace TaskSystem::v1_1::Detail