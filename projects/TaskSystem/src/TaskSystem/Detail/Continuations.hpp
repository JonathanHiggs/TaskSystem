#pragma once

#include <TaskSystem/Detail/Continuation.hpp>
#include <TaskSystem/Detail/Utils.hpp>

#include <array>
#include <vector>


namespace TaskSystem::Detail
{

    class Continuations;

    struct ContinuationSentinal
    {
    };

    class ContinuationIterator
    {
    private:
        Continuations * container;
        size_t position;

    public:
        ContinuationIterator(Continuations & container, size_t startPosition) noexcept;
        ContinuationIterator(ContinuationIterator const & other) noexcept;

        Continuation & operator*();
        Continuation * operator->();

        ContinuationIterator & operator++();
        ContinuationIterator operator++(int);

        ContinuationIterator operator+(int value) const;

        bool operator==(ContinuationIterator const & other) const;
        bool operator!=(ContinuationIterator const & other) const;

        bool operator==(ContinuationSentinal const &) const;
        bool operator!=(ContinuationSentinal const & other) const;
    };

    // Optimization: most tasks will have few continuations, a few are kept on the stack, the rest are in a vector
    class Continuations
    {
    private:
        friend class ContinuationIterator;

        std::array<Continuation, 4u> arr;
        size_t arrCount;

        std::vector<Continuation> vec;

    public:
        Continuations() noexcept;

        size_t Size() const;
        bool Empty() const;

        void Add(Continuation && continuation);

        ContinuationIterator begin();
        ContinuationSentinal end();
    };

}  // namespace TaskSystem::Detail
