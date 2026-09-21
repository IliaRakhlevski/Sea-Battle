#pragma once
/**
 * @file coord.hpp
 * @brief A cell coordinate on the board.
 *
 * Design decisions:
 *
 * - Unsigned. There are no negative coordinates on a board, and a value that
 *   was negative before conversion becomes enormous rather than negative, so
 *   the single check "less than the board size" rejects both a value that is
 *   too large and one that used to be below zero. No separate test for a
 *   negative value is needed or possible.
 *
 * - Deliberately an aggregate: no constructor, so Coords{3, 5} and a
 *   zero-initialised Coords{} both work, and the type stays trivially
 *   copyable.
 *
 * - Bounds are NOT checked here. Coords is a plain pair of numbers; whoever
 *   owns a board knows its size and does the checking. This also lets code
 *   compute an intermediate coordinate - scanning the ring around a ship,
 *   for instance - without the type objecting.
 *
 * - No operator<. It would only be needed for ordered containers of
 *   coordinates, and nothing in the game uses one; the knowledge map is a
 *   grid, not a set. Easy to add if that ever changes.
 */
#include <cstddef>

namespace seabattle {

	/** @brief Row and column of one cell, counted from zero. */
	struct Coords
	{
		std::size_t row;   ///< Row index, 0 .. board_size - 1.
		std::size_t col;   ///< Column index, 0 .. board_size - 1.

		/**
		 * @brief Two coordinates are equal when both row and column match.
		 * @param obj the coordinate to compare with.
		 * @return true if the two refer to the same cell.
		 */
		constexpr bool operator==(const Coords& obj) const
		{
			return (row == obj.row && col == obj.col);
		}

		/**
		 * @brief Negation of operator==.
		 * @param obj the coordinate to compare with.
		 * @return true if the two refer to different cells.
		 *
		 * Expressed through operator== rather than written independently, so the
		 * two can never disagree.
		 */
		constexpr bool operator!=(const Coords& obj) const
		{
			return !(*this == obj);
		}
	};

	struct Placement
	{ 
		Coords start; 
		Coords end; 
	};

} // namespace seabattle
