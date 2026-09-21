#pragma once
/**
 * @file enemy_map.hpp
 * @brief What is known about the opponent's board.
 *
 * This is NOT "the enemy board": there are no ships here and there cannot be.
 * It holds only what our own shots have revealed, plus what can be deduced
 * from them. It is the single source the targeting algorithm reads.
 *
 * Design decisions:
 *
 * - The cell states of this map and of the own board are DIFFERENT TYPES, and
 *   deliberately so. "Unknown" is impossible on one's own board and "intact
 *   deck" is impossible here, so a state that cannot occur cannot even be
 *   named. Mixing the two boards up becomes a compile error instead of a
 *   silent bug, and this header never includes own_board.hpp - that is how the
 *   fog of war is enforced here, by the dependency graph rather than by
 *   discipline.
 *
 * - record() is the only thing that changes the map, and it does the whole job:
 *   on a sinking it reconstructs the ship and crosses out the ring around it.
 *   Leaving that to the caller would allow a map in a state the rules forbid -
 *   a sunk ship whose neighbourhood is still "unknown" - and nothing would
 *   notice until a targeting algorithm wasted shots there.
 *
 * - Sunk REPLACES Hit on the cells of the ship that went down. That gives the
 *   invariant the targeting algorithm asks about every turn: any Hit cell on
 *   this map belongs to a ship that is still afloat. "Is there a wounded ship,
 *   and where" becomes a search for one state rather than a pair of conditions.
 *
 * - Missed and Crossed are separate states although both mean "no ship here".
 *   Nothing in the game distinguishes them - a shot is never worth spending on
 *   either - but the player wants to see where the shots actually went, and
 *   that is the difference between the two. Crossed is therefore written only
 *   over Unknown; a cell that was shot at keeps its Missed.
 *
 * - Nothing about the LAST shot is stored here. Whatever a strategy needs to
 *   chase a wounded ship is already on the map, and a strategy that wants to
 *   remember its own move has its own fields for it. A third copy of that fact
 *   would be a third thing to keep consistent.
 */

#include "board_area.hpp"
#include "coord.hpp"
#include "grid2d.hpp"
#include "rules.hpp"
#include "shot_result.hpp"

#include <cassert>
#include <cstddef>

namespace seabattle {

	/**
	 * @brief What is known about one cell of the opponent's board.
	 *
	 * @note There is no "a ship is here and still intact": that is precisely
	 *       what a player never learns about the opponent.
	 */
	enum class EnemyCellState
	{
		Unknown,   ///< Nothing is known about this cell; the only place worth shooting at.
		Missed,    ///< We fired here and hit water.
		Hit,       ///< A cell of a ship that is still afloat.
		Sunk,      ///< A cell of a ship that has gone down.
		Crossed    ///< Deduced to be empty - it touches a sunk ship. No shot was spent here.
	};

	/**
	 * @brief The shooter's knowledge of the opponent's board.
	 *
	 * @invariant every Hit cell belongs to a ship that is still afloat;
	 * @invariant every cell touching a Sunk ship is Missed, Crossed or Sunk -
	 *            never Unknown;
	 * @invariant sunk_ships() equals the number of separate sunk ships on the grid.
	 */
	class EnemyMap
	{
		Grid2D<EnemyCellState> cells_;   ///< One state per cell of the opponent's board.

		std::size_t sunk_ships_ = 0;     ///< How many of the opponent's ships have gone down.

	public:

		/**
		 * @brief Builds an empty map: nothing is known about any cell.
		 *
		 * Grid2D has no default constructor - it insists on being given its
		 * shape - so this constructor is not optional. Without it the compiler
		 * deletes EnemyMap's default constructor, and the class compiles
		 * happily right up to the first line that tries to create one.
		 */
		EnemyMap() : cells_(board_size, board_size, EnemyCellState::Unknown) {}

		/**
		 * @brief Writes the outcome of one shot into the map.
		 * @param at     the cell that was fired at.
		 * @param result what the opponent answered.
		 * @pre @p at is on the board and is still Unknown.
		 *
		 * A repeat shot never reaches this function: the shooter can see on
		 * this very map that the cell is not Unknown, and is expected to
		 * filter the move out before sending it. Arriving here with a cell
		 * that has already been resolved means the caller has a bug, and the
		 * assert says so rather than quietly overwriting what is known.
		 *
		 * On a sinking this does three further things - completes the ship,
		 * crosses out the ring, counts the ship - because none of them may be
		 * forgotten and all of them follow from the same event.
		 */
		void record(const Coords& at, ShotResult result)
		{
			assert(in_board(at));
			assert(cells_(at.row, at.col) == EnemyCellState::Unknown);

			switch (result)
			{
			case ShotResult::Miss:
				cells_(at.row, at.col) = EnemyCellState::Missed;
				break;

			case ShotResult::Hit:
				cells_(at.row, at.col) = EnemyCellState::Hit;
				break;

			case ShotResult::Sunk:
			{
				// Mark the cell as a hit FIRST. The reconstruction below walks
				// over Hit cells, and this one is the ship's last cell: paint
				// it Sunk straight away and the ship comes back one cell short.
				cells_(at.row, at.col) = EnemyCellState::Hit;

				const Placement ship = ship_through(at);

				fill(area_of(ship), EnemyCellState::Sunk);
				cross_ring(ship);

				++sunk_ships_;
				break;
			}
			}
			// No default label on purpose: should ShotResult ever gain a
			// value, -Wswitch points at this function instead of letting it
			// silently ignore the new case.
		}

		/**
		 * @brief The whole map, for drawing and for the targeting strategy.
		 * @return a read-only view of the grid; no copy is made.
		 *
		 * A const reference rather than a copy: the map is read every turn and
		 * drawn after every shot. The price is that Grid2D is now part of this
		 * class's public interface and cannot be swapped for something else
		 * without breaking callers.
		 */
		[[nodiscard]] const Grid2D<EnemyCellState>& cells() const noexcept
		{
			return cells_;
		}

		/**
		 * @brief How many of the opponent's ships have been sunk.
		 * @return a number between zero and ship_count.
		 *
		 * Counted as it happens rather than recomputed from the grid. A sunk
		 * four-decker leaves four Sunk cells, so counting ships would mean
		 * finding connected regions - real work, every time, for a number that
		 * changes at most ten times per game.
		 */
		[[nodiscard]] std::size_t sunk_ships() const noexcept
		{
			return sunk_ships_;
		}

	private:

		/**
		 * @brief The ship the given cell belongs to, found from the hit marks.
		 * @param from a cell that is currently Hit.
		 * @return the ship's placement, normalized.
		 *
		 * No flood fill and no recursion: a ship is a straight line, so walking
		 * outwards in the four directions while the cells are Hit finds all of
		 * it. The cell fired at may well be in the middle of the ship - the
		 * ends can have been hit earlier - which is why both directions of both
		 * axes are walked.
		 *
		 * This is correct only because ships may not touch: a Hit cell of some
		 * other ship can never be orthogonally adjacent to this one. Checked by
		 * brute force over 200000 reconstructions, with ships sunk in random
		 * order and their cells hit in random order.
		 *
		 * Each loop tests the bound BEFORE stepping. The coordinates are
		 * unsigned, so "row - 1" at row 0 is not -1 but 18446744073709551615,
		 * and that wraparound is defined behaviour no sanitizer reports.
		 */
		[[nodiscard]] Placement ship_through(const Coords& from) const
		{
			assert(cells_(from.row, from.col) == EnemyCellState::Hit);

			std::size_t first_row = from.row;
			std::size_t last_row = from.row;
			std::size_t first_col = from.col;
			std::size_t last_col = from.col;

			while (first_row > 0 && cells_(first_row - 1, from.col) == EnemyCellState::Hit)
				--first_row;

			while (last_row + 1 < board_size && cells_(last_row + 1, from.col) == EnemyCellState::Hit)
				++last_row;

			while (first_col > 0 && cells_(from.row, first_col - 1) == EnemyCellState::Hit)
				--first_col;

			while (last_col + 1 < board_size && cells_(from.row, last_col + 1) == EnemyCellState::Hit)
				++last_col;

			// A ship is one cell wide. Finding hit cells on both axes at once
			// would mean two ships touching, which the rules forbid - so this
			// fires only if the map has already been corrupted elsewhere.
			assert(first_row == last_row || first_col == last_col);

			return Placement{ Coords{ first_row, first_col }, Coords{ last_row, last_col } };
		}

		/**
		 * @brief Crosses out the cells around a ship that has just gone down.
		 * @param ship the placement of the sunk ship.
		 *
		 * Only cells that are still Unknown are written to, and that single
		 * condition does all the work: the ship's own cells are Sunk by now and
		 * stay Sunk, and a cell that was actually fired at keeps its Missed, so
		 * the player still sees where the shots went.
		 *
		 * The rectangle, and its clipping at the board edges, come from
		 * ring_around(). The placement strategy blackens the very same
		 * rectangle when it puts a ship down; one of the two copies would
		 * eventually have stopped matching the other.
		 */
		void cross_ring(const Placement& ship)
		{
			const Rect ring = ring_around(ship);

			for (std::size_t row = ring.top_left.row; row <= ring.bottom_right.row; ++row)
				for (std::size_t col = ring.top_left.col; col <= ring.bottom_right.col; ++col)
					if (cells_(row, col) == EnemyCellState::Unknown)
						cells_(row, col) = EnemyCellState::Crossed;
		}

		/**
		 * @brief Writes one state into every cell of an area.
		 * @param area  the area to overwrite; must lie on the board.
		 * @param state the state to write.
		 */
		void fill(const Rect& area, EnemyCellState state)
		{
			for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
				for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
					cells_(row, col) = state;
		}
	};

} // namespace seabattle
