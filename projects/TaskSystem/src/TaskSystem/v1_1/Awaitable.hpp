#pragma once

#include <TaskSystem/v1_1/Detail/IPromise.hpp>

#include <coroutine>


namespace TaskSystem::v1_1
{
    namespace Detail
    {

        template <typename TResult>
        struct AwaitableVTable
        {
            using destructor_m = void (*)(void *);
            using await_ready_m = bool (*)(void *);
            using await_suspend_m = std::coroutine_handle<> (*)(void *, std::coroutine_handle<>, IPromise &);
            using await_resume_m = TResult (*)(void *);

            destructor_m destructor;
            await_ready_m await_ready;
            await_suspend_m await_suspend;
            await_resume_m await_resume;
        };

        template <typename TImpl, typename TResult>
        constexpr AwaitableVTable<TResult> AwaitableFor{
            [](void * ptr) { delete static_cast<TImpl *>(ptr); },
            [](void * ptr) -> bool { return static_cast<TImpl *>(ptr)->await_ready(); },
            [](void * ptr, std::coroutine_handle<> handle, Detail::IPromise & promise) {
                return static_cast<TImpl *>(ptr)->await_suspend(handle, promise);
            },
            [](void * ptr) -> TResult {
                return static_cast<TImpl *>(ptr)->await_resume();
            }
        };

    }  // namespace Detail

    /// <summary>
    /// Type-erased awaitable
    /// </summary>
    /// <remarks>
    /// Allows ITask implementations to be awaitable when cast to ITask using a virtual call to get the
    /// implementation's awaitable type
    /// </remarks>
    /// <typeparam name="TResult">Type returned from the awaitable</typeparam>
    template <typename TResult>
    class Awaitable
    {
    private:
        void * value;
        Detail::AwaitableVTable<TResult> const * vtable;

    public:
        template <typename TImpl>
        Awaitable(TImpl const & impl)
        {
            value = new TImpl(impl);
            vtable = &Detail::AwaitableFor<TImpl, TResult>;
        }

        ~Awaitable() { vtable->destructor(value); }

        bool await_ready() { return vtable->await_ready(value); }

        template <typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> callerHandle)
        {
            Detail::IPromise & callerPromise = callerHandle.promise();
            return vtable->await_suspend(value, callerHandle, callerPromise);
        }

        TResult await_resume() { return vtable->await_resume(value); }
    };

}  // namespace TaskSystem::v1_1