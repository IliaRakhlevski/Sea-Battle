#pragma once
/**
 * @file ship.hpp
 * @brief A single ship: how long it is and how much of it is still afloat.
 *
 * Design decisions:
 *
 * - The ship does NOT store its own cells. The board's grid already maps
 *   every cell to the ship occupying it, so keeping the coordinates here as
 *   well would be the same relation stored twice - two sources of truth that
 *   can disagree. Nothing in the game asks "where is ship number three".
 *
 * - The ship's state lives in the ship and the cell's state lives in the
 *   cell. One single board method updates both together, so they cannot
 *   drift apart.
 *
 * - The counter runs DOWN, which makes is_sunk() a comparison with zero.
 *
 * - Neither field is const. A const member deletes both assignment
 *   operators, and the ships live in a std::vector inside the board - which
 *   would in turn make the whole board non-assignable and break the Rule of
 *   Zero. Immutability from the outside comes from the fields being private
 *   with no setter, not from the const keyword.
 */
#include <cstddef>
#include <cassert>
#include "rules.hpp"

namespace seabattle {

	/**
	 * @brief One ship on a player's own board.
	 *
	 * @invariant 0 <= intact_ <= size_
	 * @invariant min_ship_size <= size_ <= max_ship_size
	 */
	class Ship
	{
		std::size_t size_;     ///< Length in cells, fixed at construction.
		std::size_t intact_;   ///< Cells not yet hit; counts down to zero.

	public:

		/**
		 * @brief Builds an undamaged ship of the given length.
		 * @param ship_size length in cells; must be a length the fleet allows.
		 *
		 * explicit on purpose. Ship indices and ship lengths are both
		 * std::size_t and sit side by side in the board's code; without explicit,
		 * passing one where the other is expected compiles in silence and
		 * produces a ship of the wrong length - or, from index zero, a ship that
		 * is born already sunk.
		 */
		explicit Ship(std::size_t ship_size) : size_(ship_size), intact_(ship_size)
		{
			assert(ship_size >= min_ship_size && ship_size <= max_ship_size);
		}

		/**
		 * @brief Length of the ship in cells.
		 * @return the length it was built with; never changes.
		 */
		std::size_t size() const
		{
			return size_;
		}

		/**
		 * @brief Records a hit on one of this ship's cells.
		 * @pre the ship is not already sunk.
		 *
		 * Asserts rather than silently ignoring a hit on a sunk ship. If the
		 * board is correct a second shot at the same cell never reaches the
		 * ship, so arriving here means the caller has a bug; swallowing it would
		 * turn a visible bug into an invisible one.
		 */
		void take_hit()
		{
			assert(intact_ > 0);
			intact_--;
		}

		/**
		 * @brief Whether every cell of the ship has been hit.
		 * @return true once no intact cells remain.
		 */
		bool is_sunk() const
		{
			return (intact_ == 0);
		}
	};

} // namespace seabattle
