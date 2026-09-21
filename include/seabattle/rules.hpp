#pragma once
/**
 * @file rules.hpp
 * @brief Rules of the Russian variant of Battleship.
 *
 * Everything that would otherwise be a magic number lives here, and every
 * derived quantity is computed from the fleet rather than written down a
 * second time: a number stored twice is a number that will one day disagree
 * with itself.
 *
 * The rules were verified against three independent descriptions:
 *   - a 10x10 board;
 *   - a fleet of 1x4 + 2x3 + 3x2 + 4x1, ten ships in total;
 *   - ships may not touch, not even at the corners;
 *   - a hit earns another shot, a miss passes the turn;
 *   - the first move is decided by a coin toss.
 *
 * @note These rules differ from the Hasbro board game, which uses a 5-4-3-3-2
 *       fleet, lets ships touch, always passes the turn, and names the ship
 *       that was sunk. Two of this project's protocol decisions depend on the
 *       Russian rules and would break under the Hasbro ones - see README.
 */
#include <array>
#include <cstdint>
#include <cstddef>

namespace seabattle {

	/** @brief Side of the square board, in cells. */
	inline constexpr std::size_t board_size = 10;

	/**
	 * @brief The shortest and the longest ship in a fleet.
	 * @see ship_size_limits
	 */
	struct SizeLimits { std::size_t min; std::size_t max; };

	/**
	 * @brief One class of ship in the fleet: how long it is and how many there are.
	 *
	 * Named fields rather than a std::pair on purpose. Both members are
	 * std::size_t, so with .first and .second nothing would stop them from
	 * being swapped - and the compiler would never notice.
	 */
	struct ShipClass
	{
		std::size_t size;    ///< Ship length in cells.
		std::size_t count;   ///< How many ships of this length the fleet contains.
	};

	/**
	 * @brief The whole fleet, one entry per ship class.
	 *
	 * The array length is the number of ship CLASSES, which is unrelated to the
	 * length of the longest ship even though both happen to be 4 here.
	 */
	using Fleet = std::array<ShipClass, 4>;

	/** @brief The fleet each player must place: 1x4, 2x3, 3x2, 4x1. */
	inline constexpr Fleet fleet
	{ 
		{	{4,1}, 
			{3,2}, 
			{2,3}, 
			{1,4} 
		} 	
	};

	/**
	 * @brief Total number of ships in a fleet.
	 * @param fleet_ the fleet to measure.
	 * @return the sum of the per-class counts.
	 *
	 * Evaluated at compile time; nothing of this loop survives into the binary.
	 */
	constexpr std::size_t count_ships(const Fleet& fleet_)
	{
		std::size_t count = 0;
		for (const auto& ships : fleet_)
		{
			count += ships.count;
		}

		return count;
	}

	/**
	 * @brief How many ships a player owns, derived from @ref fleet.
	 *
	 * This is the number that decides the game: a player has lost once this
	 * many of their ships have been sunk.
	 */
	inline constexpr std::size_t ship_count = count_ships(fleet);

	/**
	 * @brief Shortest and longest ship length in a fleet.
	 * @param fleet_ the fleet to measure.
	 * @return both limits in one value, so the two cannot drift apart.
	 * @pre the fleet is not empty.
	 */
	constexpr SizeLimits ship_size_limits(const Fleet& fleet_)
	{
		SizeLimits sl{ SIZE_MAX, 0 };

		for (const auto& ships : fleet_)
		{
			if (sl.min > ships.size)
				sl.min = ships.size;

			if (sl.max < ships.size)
				sl.max = ships.size;
		}

		return sl;
	}

	/** @brief Length of the shortest ship in @ref fleet. */
	inline constexpr std::size_t min_ship_size = ship_size_limits(fleet).min;
	/** @brief Length of the longest ship in @ref fleet. */
	inline constexpr std::size_t max_ship_size = ship_size_limits(fleet).max;

} // namespace seabattle
