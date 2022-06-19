#include <TaskSystem/Detail/Promise.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Detail::Tests
{

    struct RunnablePromisePolicy
    {
        static inline constexpr bool ScheduleContinuations = false;
        static inline constexpr bool CanSchedule = true;
        static inline constexpr bool CanRun = true;
        static inline constexpr bool CanSuspend = true;
        static inline constexpr bool AllowSuspendFromCreated = false;
    };

    static_assert(PromisePolicy<RunnablePromisePolicy>);

    struct NonRunnablePromisePolicy
    {
        static inline constexpr bool ScheduleContinuations = false;
        static inline constexpr bool CanSchedule = false;
        static inline constexpr bool CanRun = false;
        static inline constexpr bool CanSuspend = false;
        static inline constexpr bool AllowSuspendFromCreated = false;
    };

    static_assert(PromisePolicy<NonRunnablePromisePolicy>);

    namespace
    {
        template <typename TResult, typename TPolicy>
        void TrySetScheduledFromCreated(bool expected)
        {
            // Arrange
            auto promise = Promise<TResult, TPolicy>();

            // A-priori
            EXPECT_EQ(promise.State(), TaskState::Created);

            // Act
            auto result = promise.TrySetScheduled();

            // Assert
            EXPECT_EQ(result, expected);
            if (expected)
            {
                EXPECT_EQ(promise.State(), TaskState::Scheduled);
            }
            else
            {
                EXPECT_EQ(promise.State(), TaskState::Created);
            }
        }
    }  // namespace

    TEST(PromiseTests, trySetScheduledFromCreated)
    {
        TrySetScheduledFromCreated<int, RunnablePromisePolicy>(true);
        TrySetScheduledFromCreated<void, RunnablePromisePolicy>(true);

        TrySetScheduledFromCreated<int, NonRunnablePromisePolicy>(false);
        TrySetScheduledFromCreated<void, NonRunnablePromisePolicy>(false);
    }

    TEST(PromiseTests, trySetScheduledFromRunningFails)
    {
        // Arrange
        auto promise = Promise<int, RunnablePromisePolicy>();

        // A-priori
        EXPECT_TRUE(promise.TrySetRunning());

        // Act
        auto result = promise.TrySetScheduled();

        // Assert
        EXPECT_FALSE(result);
    }

    TEST(PromiseTests, trySetScheduledFromSuspended)
    {
        // Arrange
        auto promise = Promise<int, RunnablePromisePolicy>();

        // A-priori
        EXPECT_TRUE(promise.TrySetRunning());
        EXPECT_TRUE(promise.TrySetSuspended());

        // Act
        auto result = promise.TrySetScheduled();

        // Assert
        EXPECT_TRUE(result);
        EXPECT_EQ(promise.State(), TaskState::Scheduled);
    }

    namespace
    {
        template <typename TResult, typename TPolicy>
        void TrySetRunningFromCreated(bool expected)
        {
            // Arrange
            auto promise = Promise<TResult, TPolicy>();

            // A-priori
            EXPECT_EQ(promise.State(), TaskState::Created);

            // Act
            auto result = promise.TrySetRunning();

            // Assert
            EXPECT_EQ(result, expected);
            if (expected)
            {
                EXPECT_EQ(promise.State(), TaskState::Running);
            }
            else
            {
                EXPECT_EQ(promise.State(), TaskState::Created);
            }
        }
    }  // namespace

    TEST(PromiseTests, trySetRunningFromCreated)
    {
        TrySetRunningFromCreated<int, RunnablePromisePolicy>(true);
        TrySetRunningFromCreated<void, RunnablePromisePolicy>(true);

        TrySetRunningFromCreated<int, NonRunnablePromisePolicy>(false);
        TrySetRunningFromCreated<void, NonRunnablePromisePolicy>(false);
    }

    namespace
    {
        template <typename TResult, typename TPolicy>
        void TrySetRunningFromScheduled()
        {
            // Arrange
            auto promise = Promise<TResult, TPolicy>();

            // A-priori
            EXPECT_TRUE(promise.TrySetScheduled());

            // Act
            auto result = promise.TrySetRunning();

            // Assert
            EXPECT_TRUE(result);
            EXPECT_EQ(promise.State(), TaskState::Running);
        }
    }  // namespace

    TEST(PromiseTests, trySetRunningFromScheduled)
    {
        TrySetRunningFromScheduled<int, RunnablePromisePolicy>();
        TrySetRunningFromScheduled<void, RunnablePromisePolicy>();
    }

    namespace
    {
        template <typename TResult, typename TPolicy>
        void TrySetSuspendedFromRunning()
        {
            // Arrange
            auto promise = Promise<TResult, TPolicy>();

            // A-priori
            EXPECT_TRUE(promise.TrySetRunning());

            // Act
            auto result = promise.TrySetSuspended();

            // Assert
            EXPECT_TRUE(result);
            EXPECT_EQ(promise.State(), TaskState::Suspended);
        }
    }  // namespace

    TEST(PromiseTests, trySetSuspendedFromRunning)
    {
        TrySetSuspendedFromRunning<int, RunnablePromisePolicy>();
        TrySetSuspendedFromRunning<void, RunnablePromisePolicy>();
    }

    namespace
    {
        template <typename TResult, typename TPolicy>
        void TrySetRunningFromSuspended()
        {
            // Arrange
            auto promise = Promise<TResult, TPolicy>();

            // A-priori
            EXPECT_TRUE(promise.TrySetRunning());
            EXPECT_TRUE(promise.TrySetSuspended());

            // Act
            auto result = promise.TrySetRunning();

            // Assert
            EXPECT_TRUE(result);
            EXPECT_EQ(promise.State(), TaskState::Running);
        }
    }  // namespace

    TEST(PromiseTests, trySetRunningFromSuspended)
    {
        TrySetRunningFromSuspended<int, RunnablePromisePolicy>();
        TrySetRunningFromSuspended<void, RunnablePromisePolicy>();
    }

    TEST(PromiseTests, trySetResultFromCreated)
    {
        // Arrange
        auto expected = 42;
        auto promise = Promise<int, RunnablePromisePolicy>();

        // Act
        auto result = promise.TrySetResult(expected);

        // Assert
        EXPECT_TRUE(result);
        EXPECT_EQ(promise.State(), TaskState::Completed);
        EXPECT_EQ(promise.Result(), expected);
    }

    TEST(PromiseTests, trySetResultFromScheduledFails)
    {
        // Arrange
        auto expected = 42;
        auto promise = Promise<int, RunnablePromisePolicy>();

        // A-priori
        EXPECT_TRUE(promise.TrySetScheduled());

        // Act
        auto result = promise.TrySetResult(expected);

        // Assert
        EXPECT_FALSE(result);
    }

    TEST(PromiseTests, trySetResultFromRunning)
    {
        // Arrange
        auto expected = 42;
        auto promise = Promise<int, RunnablePromisePolicy>();

        // A-priori
        EXPECT_TRUE(promise.TrySetRunning());

        // Act
        auto result = promise.TrySetResult(expected);

        // Assert
        EXPECT_TRUE(result);
        EXPECT_EQ(promise.State(), TaskState::Completed);
        EXPECT_EQ(promise.Result(), expected);
    }

    TEST(PromiseTests, trySetResultFromSuspended)
    {
        // Arrange
        auto expected = 42;
        auto promise = Promise<int, RunnablePromisePolicy>();

        // A-priori
        EXPECT_TRUE(promise.TrySetRunning());
        EXPECT_TRUE(promise.TrySetSuspended());

        // Act
        auto result = promise.TrySetResult(expected);

        // Assert
        EXPECT_TRUE(result);
        EXPECT_EQ(promise.State(), TaskState::Completed);
        EXPECT_EQ(promise.Result(), expected);
    }

}  // namespace TaskSystem::Detail::Tests