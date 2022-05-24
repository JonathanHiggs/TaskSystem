#pragma once

#include <utility>


namespace TaskSystem::Utils
{

    /// <summary>
    /// Tracks the number of copies and moves
    /// </summary>
    class Tracked
    {
    private:
        size_t copies = 0u;
        size_t moves = 0u;

    public:
        Tracked() = default;

        Tracked(Tracked const & other) noexcept : copies(other.copies + 1), moves(other.moves)
        { }

        Tracked & operator=(Tracked const & other) noexcept
        {
            copies = other.copies + 1;
            moves = other.moves;

            return *this;
        }

        Tracked(Tracked && other) noexcept : copies(std::exchange(other.copies, 0u)), moves(std::exchange(other.moves, 0u) + 1)
        { }

        Tracked & operator=(Tracked && other) noexcept
        {
            copies = std::exchange(other.copies, 0u);
            moves = std::exchange(other.moves, 0u) + 1;

            return *this;
        }

        size_t Copies() const
        {
            return copies;
        }

        size_t Moves() const
        {
            return moves;
        }
    };

}  // namespace TaskSystem::Utils