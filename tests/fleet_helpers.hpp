#pragma once
/**
 * @file fleet_helpers.hpp
 * @brief Small pieces several tests need: a hand-made legal fleet, and the
 *        cells a placement covers.
 */

#include "seabattle/board_area.hpp"
#include "seabattle/coord.hpp"

#include <vector>

namespace seabattle::test {

    /**
     * @brief A legal fleet laid out by hand, so a test can break it on purpose.
     *
     * One four-decker, two three-deckers, three two-deckers, four one-deckers,
     * in rows 0, 2, 4 and 6 with an empty row between them - nothing touches.
     */
    inline std::vector<Placement> legal_fleet()
    {
        return {
            { { 0, 0 }, { 0, 3 } },
            { { 2, 0 }, { 2, 2 } }, { { 2, 4 }, { 2, 6 } },
            { { 4, 0 }, { 4, 1 } }, { { 4, 3 }, { 4, 4 } }, { { 4, 6 }, { 4, 7 } },
            { { 6, 0 }, { 6, 0 } }, { { 6, 2 }, { 6, 2 } }, { { 6, 4 }, { 6, 4 } }, { { 6, 6 }, { 6, 6 } },
        };
    }

    /** @brief Every cell a normalized placement covers. */
    inline std::vector<Coords> cells_of(const Placement& ship)
    {
        std::vector<Coords> cells;
        const Rect area = area_of(ship);

        for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
            for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                cells.push_back(Coords{ row, col });

        return cells;
    }

    /** @brief Length of a normalized placement in cells. */
    inline std::size_t length_of(const Placement& ship)
    {
        return (ship.end.row - ship.start.row) + (ship.end.col - ship.start.col) + 1;
    }

} // namespace seabattle::test
