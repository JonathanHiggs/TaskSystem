#pragma once

#include <TaskSystem/Detail/IPromise.hpp>

#include <coroutine>
#include <type_traits>


namespace TaskSystem
{
    namespace Detail
    {

        template <typename T, typename = void>
        constexpr inline bool AwaitSuspendTakesHandleAndPromise = false;

        template <typename T>
        constexpr inline bool AwaitSuspendTakesHandleAndPromise<
            T,
            std::void_t<decltype(std::declval<T>().await_suspend(
                std::declval<std::coroutine_handle<>>(), std::declval<IPromise &>()))>> = true;


        template <typename T, typename = void>
        constexpr inline bool AwaitSuspendTakesHandle = false;

        template <typename T>
        constexpr inline bool AwaitSuspendTakesHandle<
            T,
            std::void_t<decltype(std::declval<T>().await_suspend(std::declval<std::coroutine_handle<>>()))>> = true;


        template <typename T, typename = void>
        constexpr inline bool AwaitSuspendTakesNothing = false;

        template <typename T>
        constexpr inline bool AwaitSuspendTakesNothing<
            T,
            std::void_t<decltype(std::declval<T>().await_suspend(std::declval<std::coroutine_handle<>>()))>> = true;


        template <typename TAwaitable, typename TResult>
        constexpr inline bool AwaitSuspendResult()
        {
            if constexpr (AwaitSuspendTakesHandleAndPromise<TAwaitable>)
            {
                return std::is_same_v<
                    decltype(std::declval<TAwaitable>()
                                 .await_suspend(std::declval<std::coroutine_handle<>>(), std::declval<IPromise &>())),
                    TResult>;
            }
            else if constexpr (AwaitSuspendTakesHandle<TAwaitable>)
            {
                return std::is_same_v<
                    decltype(std::declval<TAwaitable>().await_suspend(std::declval<std::coroutine_handle<>>())),
                    TResult>;
            }
            else if constexpr (AwaitSuspendTakesNothing<TAwaitable>)
            {
                return std::is_same_v<decltype(std::declval<TAwaitable>().await_suspend()), TResult>;
            }

            return false;
        }

        template <typename T>
        constexpr inline bool AwaitSuspendReturnsHandle = AwaitSuspendResult<T, std::coroutine_handle<>>();

        template <typename T>
        constexpr inline bool AwaitSuspendReturnsVoid = AwaitSuspendResult<T, void>();


        template <typename TImpl>
        void AwaitableDestructorAdapter(void * ptr)
        {
            delete static_cast<TImpl *>(ptr);
        }

        template <typename TImpl>
        bool AwaitReadyAdapter(void * ptr)
        {
            return static_cast<TImpl *>(ptr)->await_ready();
        }

        template <typename TImpl>
        std::coroutine_handle<> AwaitSuspendAdapter(
            void * ptr, std::coroutine_handle<> handle, Detail::IPromise & promise)
        {
            if constexpr (AwaitSuspendTakesHandleAndPromise<TImpl>)
            {
                if constexpr (AwaitSuspendReturnsHandle<TImpl>)
                {
                    return static_cast<TImpl *>(ptr)->await_suspend(handle, promise);
                }
                else
                {
                    static_cast<TImpl *>(ptr)->await_suspend(handle, promise);
                    return std::noop_coroutine();
                }
            }
            else if constexpr (AwaitSuspendTakesHandle<TImpl>)
            {
                if constexpr (AwaitSuspendReturnsHandle<TImpl>)
                {
                    return static_cast<TImpl *>(ptr)->await_suspend(handle);
                }
                else
                {
                    static_cast<TImpl *>(ptr)->await_suspend(handle);
                    return std::noop_coroutine();
                }
            }
            else
            {
                // Note: assume await_suspend doesn't take anything
                if constexpr (AwaitSuspendTakesHandle<TImpl>)
                {
                    return static_cast<TImpl *>(ptr)->await_suspend();
                }
                else
                {
                    static_cast<TImpl *>(ptr)->await_suspend();
                    return std::noop_coroutine();
                }
            }
        }

        template <typename TImpl, typename TResult>
        TResult AwaitResumeAdapter(void * ptr)
        {
            return static_cast<TImpl *>(ptr)->await_resume();
        }


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


        // Maybe: might want a concept for TImpl
        template <typename TImpl, typename TResult>
        constexpr AwaitableVTable<TResult> AwaitableFor{ &AwaitableDestructorAdapter<TImpl>,
                                                         &AwaitReadyAdapter<TImpl>,
                                                         &AwaitSuspendAdapter<TImpl>,
                                                         &AwaitResumeAdapter<TImpl, TResult> };

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

        template <Detail::PromiseType TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> callerHandle)
        {
            Detail::IPromise & callerPromise = callerHandle.promise();
            return vtable->await_suspend(value, callerHandle, callerPromise);
        }

        TResult await_resume() { return vtable->await_resume(value); }
    };

}  // namespace TaskSystem