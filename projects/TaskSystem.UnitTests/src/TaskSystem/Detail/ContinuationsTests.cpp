#pragma once

#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/Continuations.hpp>
#include <TaskSystem/Detail/Promise.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Detail::Tests
{
    namespace
    {
        struct DummyPromisePolicy final
        {
            static inline constexpr bool CanSchedule = true;
            static inline constexpr bool CanRun = true;
            static inline constexpr bool CanSuspend = true;
            static inline constexpr bool AllowSuspendFromCreated = false;
        };
    }

    TEST(ContinuationsTests, defaultInitialization)
    {
        // Act
        auto continuations = Continuations();

        // Assert
        EXPECT_EQ(continuations.Size(), 0u);
        EXPECT_TRUE(continuations.Empty());

        EXPECT_EQ(continuations.begin(), continuations.end());
        EXPECT_EQ(continuations.begin() + 1, continuations.end());
    }

    TEST(ContinuationsTests, addOne)
    {
        // Arrange
        auto continuations = Continuations();

        // Act
        continuations.Add(Continuation());

        // Assert
        EXPECT_EQ(continuations.Size(), 1u);
        EXPECT_FALSE(continuations.Empty());

        EXPECT_NE(continuations.begin(), continuations.end());
        EXPECT_EQ(++continuations.begin(), continuations.end());
        EXPECT_EQ(continuations.begin() + 1, continuations.end());
    }

    TEST(ContinuationsTests, addSix)
    {
        // Arrange
        auto continuations = Continuations();

        // Act
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});

        // Assert
        EXPECT_EQ(continuations.Size(), 6u);
        EXPECT_FALSE(continuations.Empty());

        EXPECT_NE(continuations.begin() + 0, continuations.end());
        EXPECT_NE(continuations.begin() + 1, continuations.end());
        EXPECT_NE(continuations.begin() + 2, continuations.end());
        EXPECT_NE(continuations.begin() + 3, continuations.end());
        EXPECT_NE(continuations.begin() + 4, continuations.end());
        EXPECT_NE(continuations.begin() + 5, continuations.end());
        EXPECT_EQ(continuations.begin() + 6, continuations.end());
    }

    TEST(ContinuationsTests, accessElement)
    {
        // Arrange
        auto continuations = Continuations();
        auto promise = Promise<int, DummyPromisePolicy>();

        // Act
        continuations.Add(Continuation(promise));

        // Assert
        EXPECT_EQ(&continuations.begin()->Promise(), &promise);
    }

    TEST(ContinuationsTests, accessInvalidElement)
    {
        // Arrange
        auto continuations = Continuations();

        // Act
        continuations.Add(Continuation());

        // Assert
        EXPECT_THROW((++continuations.begin())->Promise(), std::exception);
    }

    TEST(ContinuationsTests, accessElementInVec)
    {
        // Arrange
        auto continuations = Continuations();
        auto promise0 = Promise<int, DummyPromisePolicy>();
        auto promise1 = Promise<int, DummyPromisePolicy>();
        auto promise2 = Promise<int, DummyPromisePolicy>();
        auto promise3 = Promise<int, DummyPromisePolicy>();
        auto promise4 = Promise<int, DummyPromisePolicy>();
        auto promise5 = Promise<int, DummyPromisePolicy>();

        // Act
        continuations.Add(Continuation(promise0));
        continuations.Add(Continuation(promise1));
        continuations.Add(Continuation(promise2));
        continuations.Add(Continuation(promise3));
        continuations.Add(Continuation(promise4));
        continuations.Add(Continuation(promise5));

        // Assert
        auto it = continuations.begin();

        EXPECT_EQ(&(it++)->Promise(), &promise0);
        EXPECT_EQ(&(it++)->Promise(), &promise1);
        EXPECT_EQ(&(it++)->Promise(), &promise2);
        EXPECT_EQ(&(it++)->Promise(), &promise3);
        EXPECT_EQ(&(it++)->Promise(), &promise4);
        EXPECT_EQ(&(it++)->Promise(), &promise5);
    }

    TEST(ContinuationsTests, forLoopCountsElements)
    {
        // Arrange
        auto continuations = Continuations();

        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});
        continuations.Add(Continuation{});

        // Act
        auto count = 0u;
        for ([[maybe_unused]] auto const & _ : continuations)
        {
            ++count;
        }

        // Assert
        EXPECT_EQ(count, continuations.Size());
    }

}