#pragma once
/**
 * @file own_board.hpp
 * @brief A player's own board: where the ships are and what has been hit.
 *
 * Design decisions:
 *
 * - Cells and ships are linked BY INDEX, not by pointer. Verified under
 *   AddressSanitizer: std::vector moves its elements when it reallocates, so
 *   pointers into it dangle; and when the board is copied, pointers would
 *   still refer to the original's ships, which would force all five special
 *   member functions to be written by hand. With an index the class stays on
 *   the Rule of Zero.
 *
 * - A cell is a std::variant, not a status plus an always-present ship index.
 *   Water carrying a ship index cannot be expressed at all, rather than being
 *   forbidden by a comment.
 *
 * - The whole fleet is deployed in ONE call. A per-ship method could not check
 *   that the list matches the fleet - it sees one ship at a time and has no
 *   idea how many three-deckers have already gone by - and a refusal halfway
 *   through would leave the board half-filled, with somebody having to undo
 *   it. deploy() validates everything first and writes nothing until it is
 *   sure, so the board is either complete or untouched.
 *
 * - deploy() re-checks what the placement strategy already checked. That is
 *   deliberate: a strategy can be written by anyone, and the manual one is a
 *   human, who will get it wrong. The board does not trust what arrives from
 *   outside.
 *
 * - receive_shot() is the only method that changes a cell and a ship together,
 *   which is why the two halves of that state cannot drift apart.
 */

#include "board_area.hpp"
#include "coord.hpp"
#include "grid2d.hpp"
#include "rules.hpp"
#include "ship.hpp"
#include "shot_result.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace seabattle {

    /** @brief Condition of a water cell on the owner's board. */
    enum class WaterState
    {
        Untouched,   ///< Water; nobody has fired here yet.
        Missed       ///< Water; the opponent fired here and missed.
    };

    /** @brief Condition of one cell of a ship. */
    enum class DeckState
    {
        Intact,      ///< This cell of the ship has not been hit.
        Hit          ///< This cell of the ship has been hit.
    };

    /** @brief A cell holding water. */
    struct WaterCell
    {
        WaterState state;
    };

    /** @brief A cell occupied by one ship. */
    struct ShipCell
    {
        DeckState   state;   ///< Whether this particular cell has been hit.
        std::size_t ship;    ///< Index into the board's ship vector.
    };

    /** @brief One cell of the owner's board: either water or part of a ship. */
    using Cell = std::variant<WaterCell, ShipCell>;

    /**
     * @brief The board a player defends.
     *
     * @invariant every ShipCell's index addresses an existing ship;
     * @invariant a ship's intact count equals the number of its cells that are
     *            still DeckState::Intact;
     * @invariant either no ship is deployed at all, or the whole fleet is.
     */
    class OwnBoard
    {
        Grid2D<Cell> cells_;        ///< What each cell of the board holds.
        std::vector<Ship> ships_;   ///< The fleet; empty until deploy() succeeds.

    public:

        /** @brief Builds an empty board: all water, no ships. */
        OwnBoard() : cells_(board_size, board_size, Cell(WaterCell{ WaterState::Untouched })) {}

        /**
         * @brief Puts a whole fleet on the board.
         * @param placements one placement per ship, in any order.
         * @return true if the list was legal and the board now holds the fleet.
         *
         * Nothing is written until the entire list has been checked, so a
         * refusal leaves the board exactly as it was. What is checked:
         *
         *   - the board is still empty - a fleet is deployed once;
         *   - there are exactly ship_count placements;
         *   - each one lies on the board, is ordered start-to-end, and is one
         *     cell wide;
         *   - the lengths add up to the fleet: one four-decker, two
         *     three-deckers, three two-deckers, four one-deckers;
         *   - no two ships overlap or touch, not even at a corner.
         */
        [[nodiscard]] bool deploy(const std::vector<Placement>& placements)
        {
            if (!ships_.empty() || placements.size() != ship_count)
                return false;

            // Built aside and swapped in at the very end. This is what makes
            // the operation all-or-nothing without any rollback code.
            Grid2D<Cell> cells(board_size, board_size, Cell(WaterCell{ WaterState::Untouched }));
            std::vector<Ship> ships;
            ships.reserve(ship_count);

            // How many ships of each length the list contains. Index is the
            // length itself, so index 0 stays unused.
            std::array<std::size_t, max_ship_size + 1> by_length{};

            for (const Placement& ship : placements)
            {
                if (!is_shape_legal(ship))
                    return false;

                const std::size_t len = length_of(ship);

                if (len < min_ship_size || len > max_ship_size)
                    return false;

                // The ring, not just the ship: a neighbour one cell away - or
                // diagonally - is already a violation. Only ships already
                // written into this scratch board can be found there.
                if (!is_clear_of_ships(cells, ring_around(ship)))
                    return false;

                ++by_length[len];

                const std::size_t index = ships.size();
                ships.emplace_back(len);

                const Rect area = area_of(ship);
                for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
                    for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                        cells(row, col) = Cell(ShipCell{ DeckState::Intact, index });
            }

            for (const ShipClass& ship_class : fleet)
                if (by_length[ship_class.size] != ship_class.count)
                    return false;

            cells_ = std::move(cells);
            ships_ = std::move(ships);

            return true;
        }

        /**
         * @brief Takes one shot from the opponent.
         * @param at the cell being fired at.
         * @return what happened, or nothing if the shot is refused.
         * @pre a fleet has been deployed.
         *
         * A shot is refused when the cell is off the board or has already been
         * resolved. That is the board declining to answer a question it has
         * answered before - it is not one of the three outcomes, which is why
         * it travels in the return type instead. The shooter is expected to
         * never ask twice: its own knowledge map already tells it so.
         *
         * This is the only place where a cell and a ship change together.
         */
        [[nodiscard]] std::optional<ShotResult> receive_shot(const Coords& at)
        {
            assert(is_deployed());

            if (!in_board(at))
                return std::nullopt;

            Cell& cell = cells_(at.row, at.col);

            if (WaterCell* water = std::get_if<WaterCell>(&cell))
            {
                if (water->state == WaterState::Missed)
                    return std::nullopt;

                water->state = WaterState::Missed;
                return ShotResult::Miss;
            }

            ShipCell& deck = std::get<ShipCell>(cell);

            if (deck.state == DeckState::Hit)
                return std::nullopt;

            deck.state = DeckState::Hit;

            Ship& ship = ships_[deck.ship];
            ship.take_hit();

            return ship.is_sunk() ? ShotResult::Sunk : ShotResult::Hit;
        }

        /**
         * @brief Whether the ship occupying a cell has been sunk.
         * @param at the cell to ask about.
         * @return true only if that cell belongs to a ship with no intact cells left.
         * @pre @p at is on the board.
         *
         * Water answers false, and so does a cell of a ship still afloat.
         *
         * This is a QUESTION, not a new piece of state. Whether a ship is sunk
         * is known by the ship and nowhere else; storing it in the cell as well
         * would be the same fact in two places, and the one who drew the board
         * would be the first to notice them disagreeing. The owner needs the
         * answer because on their own board a damaged deck and a dead one look
         * alike otherwise.
         */
        [[nodiscard]] bool is_sunk_at(const Coords& at) const
        {
            assert(in_board(at));

            const ShipCell* deck = std::get_if<ShipCell>(&cells_(at.row, at.col));

            return deck != nullptr && ships_[deck->ship].is_sunk();
        }

        /** @brief Whether a fleet has been deployed on this board. */
        [[nodiscard]] bool is_deployed() const noexcept
        {
            return !ships_.empty();
        }

        /** @brief How many of this player's ships have been sunk. */
        [[nodiscard]] std::size_t sunk_ships() const
        {
            std::size_t sunk = 0;

            for (const Ship& ship : ships_)
                if (ship.is_sunk())
                    ++sunk;

            return sunk;
        }

        /**
         * @brief Whether every ship on this board has been sunk.
         *
         * A board with no fleet on it has not lost; it has not started.
         */
        [[nodiscard]] bool is_defeated() const
        {
            return is_deployed() && sunk_ships() == ships_.size();
        }

        /**
         * @brief The whole board, for drawing.
         * @return a read-only view of the grid; no copy is made.
         */
        [[nodiscard]] const Grid2D<Cell>& cells() const noexcept
        {
            return cells_;
        }

    private:

        /**
         * @brief Whether a placement is on the board, ordered, and one cell wide.
         * @param ship the placement to check.
         *
         * A diagonal placement and one whose end comes before its start are both
         * expressible - Placement is a plain pair of coordinates with no
         * constructor - so both have to be rejected here.
         */
        [[nodiscard]] static bool is_shape_legal(const Placement& ship) noexcept
        {
            if (!in_board(ship.start) || !in_board(ship.end))
                return false;

            if (ship.start.row > ship.end.row || ship.start.col > ship.end.col)
                return false;

            return ship.start.row == ship.end.row || ship.start.col == ship.end.col;
        }

        /**
         * @brief Length of a placement in cells.
         * @param ship a placement that has passed is_shape_legal().
         *
         * One of the two differences is zero, so adding them and adding one
         * gives the length whichever way the ship runs.
         */
        [[nodiscard]] static std::size_t length_of(const Placement& ship) noexcept
        {
            return (ship.end.row - ship.start.row) + (ship.end.col - ship.start.col) + 1;
        }

        /**
         * @brief Whether an area holds no ship cell at all.
         * @param cells the board being built.
         * @param area  the area to inspect; must lie on the board.
         */
        [[nodiscard]] static bool is_clear_of_ships(const Grid2D<Cell>& cells, const Rect& area)
        {
            for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
                for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                    if (std::holds_alternative<ShipCell>(cells(row, col)))
                        return false;

            return true;
        }
    };

} // namespace seabattle
