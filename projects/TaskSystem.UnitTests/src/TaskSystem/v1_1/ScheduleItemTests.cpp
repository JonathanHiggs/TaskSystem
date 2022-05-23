#include <TaskSystem/v1_1/ScheduleItem.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::v1_1::Tests
{
    namespace
    {
        void EmptyFunction()
        { }

        void ThrowFunction()
        {
            throw std::exception();
        }

    }  // namespace

    TEST(ScheduleItemTests, runLambda)
    {
        // Arrange
        auto completed = false;
        auto lambda = [&]() { completed = true; };

        // Act
        auto item = ScheduleItem(std::move(lambda));
        auto result = item.Run();

        // Assert
        EXPECT_EQ(result, nullptr);
        EXPECT_TRUE(completed);
    }

    TEST(ScheduleItemTests, runLambdaThrows)
    {
        // Arrange
        auto lambda = [&]() { throw std::exception(); };

        // Act
        auto item = ScheduleItem(std::move(lambda));
        auto result = item.Run();

        // Assert
        EXPECT_NE(result, nullptr);
    }

    TEST(ScheduleItemTests, runFunction)
    {
        // Act
        auto item = ScheduleItem(EmptyFunction);
        auto result = item.Run();

        // Assert
        EXPECT_EQ(result, nullptr);
    }

    TEST(ScheduleItemTests, runFunctionThrows)
    {
        // Act
        auto item = ScheduleItem(ThrowFunction);
        auto result = item.Run();

        // Assert
        EXPECT_NE(result, nullptr);
    }

}  // namespace TaskSystem::v1_1::Tests