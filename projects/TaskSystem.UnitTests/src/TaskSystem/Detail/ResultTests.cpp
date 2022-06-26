#include <TaskSystem/Detail/Result.hpp>

#include <gtest/gtest.h>


namespace TaskSystem::Detail::Tests
{
    class ResultError
    {
    public:
        enum ValueType
        {
            Reason1,
            Reason2
        };

    private:
        ValueType value;

    public:
        constexpr ResultError(ValueType const value) noexcept : value(value) {}

        [[nodiscard]] constexpr operator ValueType() const { return value; }

        explicit operator bool() = delete;
    };

    using ResultT = Result<ResultError>;


    TEST(ResultTests, defaultCtor)
    {
        // Arrange
        auto result = ResultT();

        // Act
        auto success = static_cast<bool>(result);

        // Assert
        EXPECT_TRUE(success);
    }

    TEST(ResultTests, returnSuccess)
    {
        // Arrange
        auto fn = []() -> ResultT { return Success; };

        // Act
        auto result = fn();

        // Assert
        EXPECT_TRUE(result);
    }

    TEST(ResultTests, returnError)
    {
        // Arrange
        auto fn = []() -> ResultT { return ResultError::Reason1; };

        // Act
        auto result = fn();

        // Assert
        EXPECT_FALSE(result);
        EXPECT_EQ(*result, ResultError::Reason1);
    }

    TEST(ResultTests, comparisons)
    {
        // Arrange
        ResultT result = ResultError::Reason1;

        // Act && Assert
        EXPECT_EQ(result, ResultError::Reason1);
        EXPECT_EQ(result, ResultError(ResultError::Reason1));
        EXPECT_EQ(ResultError::Reason1, result);
        EXPECT_EQ(ResultError(ResultError::Reason1), result);

        EXPECT_NE(result, ResultError::Reason2);
        EXPECT_NE(result, ResultError(ResultError::Reason2));
        EXPECT_NE(ResultError::Reason2, result);
        EXPECT_NE(ResultError(ResultError::Reason2), result);

        EXPECT_EQ(result, false);
        EXPECT_NE(result, true);
        EXPECT_EQ(false, result);
        EXPECT_NE(true, result);
    }

}