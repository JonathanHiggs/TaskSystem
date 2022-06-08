#pragma once

#include <TaskSystem/v1_1/Detail/IPromise.hpp>

#include <coroutine>


namespace TaskSystem::v1_1
{
    namespace Detail
    {

        // clang-format off
        template <typename TResult>
        struct AwaitableVTable
        {
            void                    (* destroy)      (void * ptr);
            bool                    (* await_ready)  (void * ptr);
            std::coroutine_handle<> (* await_suspend)(void * ptr, std::coroutine_handle<>, Detail::IPromise & promise);
            TResult                 (* await_resume) (void * ptr);
        };

        template <typename TImpl, typename TResult>
        constexpr AwaitableVTable<TResult> AwaitableFor
        {
            [](void * ptr)                                { delete static_cast<TImpl *>(ptr); },
            [](void * ptr) -> bool                        { return static_cast<TImpl *>(ptr)->await_ready(); },
            [](void * ptr, std::coroutine_handle<> handle, Detail::IPromise & promise)
                                                          { return static_cast<TImpl*>(ptr)->await_suspend(handle, promise); },
            [](void * ptr) -> TResult                     { return static_cast<TImpl *>(ptr)->await_resume(); }
        };
        // clang-format on

    }  // namespace Detail

    // Type-erased awaitable
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

        ~Awaitable() { vtable->destroy(value); }

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