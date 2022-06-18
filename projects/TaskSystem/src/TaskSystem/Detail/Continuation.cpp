#include <TaskSystem/Detail/Continuation.hpp>

#include <TaskSystem/Detail/IPromise.hpp>


namespace TaskSystem::Detail
{

    Continuation::operator bool() const noexcept { return promise && promise->Handle() != nullptr; }

    std::coroutine_handle<> Continuation::Handle() { return promise->Handle(); }

}