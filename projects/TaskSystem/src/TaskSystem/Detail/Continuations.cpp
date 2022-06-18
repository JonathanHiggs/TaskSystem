#include <TaskSystem/Detail/Continuations.hpp>


namespace TaskSystem::Detail
{

    ContinuationIterator::ContinuationIterator(Continuations & container, size_t startPosition) noexcept
        : container(&container), position(startPosition)
    { }

    ContinuationIterator::ContinuationIterator(ContinuationIterator const & other) noexcept
        : container(other.container), position(other.position)
    { }

    Continuation & ContinuationIterator::operator*()
    {
        if (position >= container->arrCount + container->vec.size()) [[unlikely]]
        {
            throw std::exception();
        }

            if (position < container->arrCount) [[likely]]
            {
                return container->arr[position];
            }

        return container->vec[position - container->arr.size()];
    }

    Continuation * ContinuationIterator::operator->() { return &(operator*()); }

    ContinuationIterator & ContinuationIterator::operator++()
    {
        ++position;
        return *this;
    }

    ContinuationIterator ContinuationIterator::operator++(int)
    {
        ContinuationIterator temp(*this);
        ++(*this);
        return temp;
    }

    ContinuationIterator ContinuationIterator::operator+(int value) const
    {
        return ContinuationIterator(*container, position + value);
    }

    bool ContinuationIterator::operator==(ContinuationIterator const & other) const
    {
        return container == other.container && position == other.position;
    }

    bool ContinuationIterator::operator!=(ContinuationIterator const & other) const { return !(*this == other); }

    bool ContinuationIterator::operator==(ContinuationSentinal const &) const
    {
        return position >= container->arrCount + container->vec.size();
    }

    bool ContinuationIterator::operator!=(ContinuationSentinal const & other) const { return !(*this == other); }


    Continuations::Continuations() noexcept : arr(), arrCount(0u), vec(0u) { }

    size_t Continuations::Size() const { return arrCount + vec.size(); }
    bool Continuations::Empty() const { return Size() == 0u; }

    void Continuations::Add(Continuation && continuation)
    {
        if (arrCount != arr.size()) [[likely]]
        {
            arr[arrCount] = std::move(continuation);
        ++arrCount;
        }
        else
        {
            vec.emplace_back(std::move(continuation));
        }
    }

    ContinuationIterator Continuations::begin() { return ContinuationIterator(*this, 0u); }
    ContinuationSentinal Continuations::end() { return ContinuationSentinal(); }

}  // namespace TaskSystem::Detail
