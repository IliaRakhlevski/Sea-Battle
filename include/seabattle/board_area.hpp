#pragma once
/**
 * @file board_area.hpp
 * @brief Geometry on the board: rectangles, bounds, the ring around a ship.
 *
 * Why this file exists:
 *
 * - The work it does needs both halves of the problem at once: the types from
 *   coord.hpp and the board size from rules.hpp. Those two headers are
 *   deliberately independent of each other, and neither is the right place to
 *   break that. coord.hpp is plain geometry, true for any game; rules.hpp is
 *   the rules of this one. A dependency from the rules to the geometry would
 *   be fine, the reverse would not - so instead of bending either, this third
 *   header includes both and nobody else's dependencies change.
 *
 * - It is also the home coord.hpp already points at. That file says bounds are
 *   not checked there, because "whoever owns a board knows its size and does
 *   the checking". in_board() is that check, finally written down once instead
 *   of being spelled out again in every caller.
 *
 * - Two users, arriving from opposite directions. The placement strategy marks
 *   the ring around a ship it has just placed so that no later ship can touch
 *   it. The knowledge map crosses out the ring around a ship it has just sunk,
 *   because those cells cannot hold anything. Same rectangle, same clipping at
 *   the edges, opposite meanings - and the two must never include each other,
 *   since that is how the fog of war is enforced here.
 */

#include "coord.hpp"
#include "rules.hpp"

#include <cstddef>

namespace seabattle {

    /**
     * @brief A rectangle of cells, both corners included.
     *
     * Distinct from Placement on purpose, even though the two hold the same
     * two coordinates. A Placement is where a SHIP lies; a Rect is an area,
     * and the most useful ones - the ring around a ship - contain no ship at
     * all. Giving them one name would mean writing "Placement ring", which is
     * exactly the kind of sentence that stops a reader.
     */
    struct Rect
    {
        Coords top_left;       ///< Smallest row and column of the area.
        Coords bottom_right;   ///< Largest row and column, inclusive.
    };

    /**
     * @brief Whether a coordinate lies on the board.
     * @param cell the coordinate to test.
     * @return true if both indices are below board_size.
     *
     * One comparison per axis is enough, and there is no separate test for a
     * negative value: the coordinates are unsigned, so a value that was
     * negative before conversion has become enormous and fails the same test.
     */
    constexpr bool in_board(const Coords& cell) noexcept
    {
        return cell.row < board_size && cell.col < board_size;
    }

    /**
     * @brief The area a ship itself occupies.
     * @param ship a normalized placement - start no greater than end on both axes.
     * @return the same two corners, read as an area.
     * @pre the placement is normalized and lies on the board.
     */
    constexpr Rect area_of(const Placement& ship) noexcept
    {
        return Rect{ ship.start, ship.end };
    }

    /**
     * @brief The ship together with the one-cell ring around it, clipped to the board.
     * @param ship a normalized placement lying on the board.
     * @return an area one cell wider on every side, cut off at the edges.
     * @pre the placement is normalized and lies on the board.
     *
     * This is the single place that knows what "ships may not touch, not even
     * at the corners" means in coordinates. Whoever calls it decides what to
     * do with the area; the shape of the area is decided here.
     *
     * The clipping is computed BEFORE the arithmetic, not repaired after it.
     * In row 0, "row - 1" is not -1 but 18446744073709551615: the coordinates
     * are unsigned, the wraparound is defined behaviour, and a check written
     * afterwards would have nothing left to look at. Neither the compiler nor
     * UndefinedBehaviorSanitizer reports it.
     */
    constexpr Rect ring_around(const Placement& ship) noexcept
    {
        const std::size_t first_row = (ship.start.row == 0) ? 0 : ship.start.row - 1;
        const std::size_t first_col = (ship.start.col == 0) ? 0 : ship.start.col - 1;

        const std::size_t last_row = (ship.end.row + 1 < board_size) ? ship.end.row + 1
                                                                    : board_size - 1;
        const std::size_t last_col = (ship.end.col + 1 < board_size) ? ship.end.col + 1
                                                                    : board_size - 1;

        return Rect{ Coords{ first_row, first_col }, Coords{ last_row, last_col } };
    }

} // namespace seabattle
