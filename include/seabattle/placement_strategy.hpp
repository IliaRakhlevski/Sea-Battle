#pragma once
/**
 * @file placement_strategy.hpp
 * @brief How a fleet gets placed on a board: automatically or by a human.
 *
 * Design decisions:
 *
 * - A placement strategy returns a LIST OF PLACEMENTS; it never touches a
 *   board. That keeps this header independent of own_board.hpp, and keeps
 *   own_board.hpp independent of this one - the two never include each other.
 *   Player is what brings them together.
 *
 * - A strategy therefore needs nothing but the rules and the coordinates:
 *   board size and fleet composition come from rules.hpp, and one placement
 *   is a Placement from coord.hpp. In particular it does NOT need the
 *   knowledge map, which is why targeting lives in its own header.
 *
 * - The consequence to keep in mind: the board cannot trust the list it is
 *   handed and validates it anyway. The rules are therefore checked in two
 *   places unless the check itself is factored into a shared free function.
 *
 * - Randomness must be reproducible. A strategy that seeds its own generator
 *   internally makes a failing game impossible to replay, which is exactly
 *   when a replay is needed. The seed comes from outside.
 *
 * - Random placement can fail to find room for a ship. The number of attempts
 *   is bounded, and running out of attempts is a normal outcome that has to be
 *   reported, not an exception.
 */

#include "board_area.hpp"
#include "coord.hpp"
#include "rules.hpp"
#include "grid2d.hpp"
#include <vector>
#include <optional>
#include <random>
#include <algorithm>
#include <cstddef>
#include <cassert>

namespace seabattle {

    /**
     * @brief Whether a cell of the scratch grid may still be used.
     *
     * The scratch grid is local to the placement algorithm and has nothing to
     * do with the cells of a real board: it only remembers which squares are
     * no longer available. A cell is Taken either because a ship sits on it or
     * because it touches one - the rule "ships may not touch, not even at the
     * corners" is enforced by marking the ring around every ship as Taken, so
     * that afterwards a candidate only has to test its own cells.
     *
     * Not Grid2D<bool>: std::vector<bool> is the bit-packed specialisation,
     * its operator[] returns a proxy object, and Grid2D's "T& ref(...)" cannot
     * bind a reference to it. Grid2D<Busy> is an ordinary vector and costs the
     * same single byte per cell.
     */
    enum class Busy { Free, Taken };

    /**
     * @brief The direction in which a ship extends from its starting cell.
     *
     * @note All four values are accepted, and the draw stays uniform over the
     *       set of possible segments: North produces exactly the same vertical
     *       segments as South and West the same horizontal ones as East, each
     *       one equally likely. Two of the four are therefore redundant; they
     *       are kept because a manual strategy will want to say "place it
     *       upwards from here" in the player's own terms.
     */
    enum class Orientation { North, South, West, East };

    /**
     * @brief Where every ship of a fleet goes, or nothing.
     *
     * Empty means the strategy could not produce a legal arrangement: the
     * random one ran out of attempts, or a human gave up. It is an ordinary
     * outcome the caller has to handle, not an error condition.
     */
    using PlacementResult = std::optional<std::vector<Placement>>;

    /** @brief The interface every placement strategy implements. */
    struct IPlacementStrategy {
            virtual ~IPlacementStrategy() = default;

            /**
             * @brief Produces one complete arrangement of the fleet.
             * @return one Placement per ship, in fleet order, or nothing.
             */
            [[nodiscard]] virtual PlacementResult make_placement() = 0;
    };

    /**
     * @brief Helpers of the placement algorithm. Not part of the public API.
     *
     * Free functions rather than members: none of them needs the generator or
     * any other state, and keeping them out of the class makes each one
     * testable on its own with a hand-built grid.
     */
    namespace detail {

        // A ship is never longer than the board is wide; the arithmetic below
        // subtracts (length - 1) from the board size and would wrap around if
        // this were ever false.
        static_assert(max_ship_size <= board_size,
                      "A ship longer than the board cannot be placed");

        /** @brief One step from a cell towards the far end of the ship. */
        struct Step
        {
            int row;   ///< -1 upwards, +1 downwards, 0 if the ship is horizontal.
            int col;   ///< -1 leftwards, +1 rightwards, 0 if the ship is vertical.
        };

        /**
         * @brief The unit step belonging to an orientation.
         * @param dir the direction the ship extends in.
         * @return the row and column increment of one cell in that direction.
         */
        constexpr Step step_of(Orientation dir) noexcept
        {
            switch (dir)
            {
            case Orientation::North: return { -1,  0 };
            case Orientation::South: return { +1,  0 };
            case Orientation::West:  return {  0, -1 };
            case Orientation::East:  return {  0, +1 };
            }

            return { 0, 0 };   // unreachable for a valid enumerator
        }

        /** @brief An inclusive range of indices along one axis. */
        struct Range
        {
            std::size_t lo;   ///< Smallest allowed index.
            std::size_t hi;   ///< Largest allowed index, inclusive.
        };

        /**
         * @brief Indices a ship may start at on one axis so that it still fits.
         * @param len  length of the ship in cells; at least one.
         * @param step this axis's component of the unit step: -1, 0 or +1.
         * @return the inclusive range of valid start indices on that axis.
         *
         * The ship reaches (len - 1) cells beyond its start along the axis it
         * runs on, and none at all along the other one. Growing towards higher
         * indices therefore costs that much room at the far end of the axis,
         * growing towards lower indices costs it at the near end.
         *
         * This is the only place that knows the relation between a length and
         * the room it needs. far_end() below uses the same step, so a start
         * drawn from this range can never run off the board and no second
         * bounds check is needed anywhere else.
         */
        constexpr Range start_range(std::size_t len, int step) noexcept
        {
            const std::size_t span = (step == 0) ? 0 : len - 1;

            if (step < 0)
                return { span, board_size - 1 };

            return { 0, board_size - 1 - span };
        }

        /**
         * @brief The cell at the other end of a ship.
         * @param start first cell of the ship.
         * @param len   length of the ship in cells; at least one.
         * @param step  the unit step of its orientation.
         * @return the last cell the ship occupies.
         * @pre @p start comes from start_range() for the same @p len and @p step.
         *
         * Three branches rather than "start.row + step.row * span". The reason is
         * NOT safety: measured, with the precondition broken - start column 0, a
         * ship of four running west - the branches and the multiplication return
         * the very same 18446744073709551613. Unsigned wraparound is defined
         * behaviour, so nothing diagnoses either of them: not -Wall -Wextra, not
         * -Wconversion, not UndefinedBehaviorSanitizer. What keeps this function
         * correct is the precondition alone.
         *
         * The reason is that the multiplication mixes int and std::size_t. Left
         * as written it is the one thing here that -Wsign-conversion does report;
         * silenced with a static_cast, that cast then also hides every other sign
         * mistake later made on the same line. The branches need no cast, so
         * nothing has to be silenced.
         */
        constexpr Coords far_end(const Coords& start, std::size_t len, Step step) noexcept
        {
            const std::size_t span = len - 1;

            Coords end = start;

            if (step.row > 0)       end.row += span;
            else if (step.row < 0)  end.row -= span;

            if (step.col > 0)       end.col += span;
            else if (step.col < 0)  end.col -= span;

            return end;
        }

        /**
         * @brief The same two cells, reordered so that start <= end on both axes.
         * @param a one end of the segment.
         * @param b the other end.
         * @return a Placement whose start is the upper-left of the two.
         *
         * A ship running North or West is generated back to front. Normalising
         * here means every Placement that leaves this header reads the same way
         * round, and everything downstream - the board, the tests, the renderer -
         * can walk a ship with a plain "for row from start to end" loop instead
         * of each of them working out which end is which.
         *
         * Both members are compared independently, which is only correct because
         * a ship is axis-aligned: on one of the two axes the values are equal,
         * so min and max both return that same value.
         */
        constexpr Placement normalized(const Coords& a, const Coords& b) noexcept
        {
            return Placement{ Coords{ (a.row < b.row) ? a.row : b.row,
                                      (a.col < b.col) ? a.col : b.col },
                              Coords{ (a.row > b.row) ? a.row : b.row,
                                      (a.col > b.col) ? a.col : b.col } };
        }

        /**
         * @brief Whether every cell of an area is still available.
         * @param cells the scratch grid.
         * @param area  the area to test; for a ship, one cell wide.
         * @return true if no cell of the area is Taken.
         * @pre the area lies inside the grid.
         *
         * Called with the ship's own cells and nothing more. The gap the rules
         * demand is already accounted for, because mark_with_ring() blackens
         * the ring around each ship as it is placed.
         */
        inline bool is_area_free(const Grid2D<Busy>& cells, const Rect& area)
        {
            for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
                for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                    if (cells(row, col) != Busy::Free)
                        return false;

            return true;
        }

        /**
         * @brief Marks a ship and the ring of cells around it as Taken.
         * @param cells the scratch grid, modified in place.
         * @param ship  a normalized placement that has been accepted.
         * @pre the placement lies inside the grid.
         *
         * This is what turns "ships may not touch, not even at the corners"
         * into a property of the grid: once the ring is Taken, any later ship
         * that would touch this one fails the plain is_area_free() test.
         *
         * The shape of the ring - and the clipping at the board edges, where
         * the unsigned arithmetic has a mine in it - belongs to
         * ring_around() in board_area.hpp. The knowledge map crosses out the
         * very same rectangle after a sinking, and one of the two copies would
         * eventually have stopped matching the other.
         */
        inline void mark_with_ring(Grid2D<Busy>& cells, const Placement& ship)
        {
            const Rect area = ring_around(ship);

            for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
                for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                    cells(row, col) = Busy::Taken;
        }

    } // namespace detail

    /**
     * @brief Places the whole fleet at random.
     *
     * The algorithm is "draw and reject", on two levels. One ship is offered a
     * bounded number of random positions until one of them fits. A whole fleet
     * is attempted a bounded number of times, because the ships are placed one
     * after another and an unlucky early ship can leave no room for a later
     * one - a state no amount of retrying the LAST ship can escape.
     *
     * Both limits are generous for a 10x10 board and the standard fleet; they
     * exist so that a malformed set of rules cannot turn into an endless loop.
     */
    class AutomaticPlacementStrategy : public IPlacementStrategy
    {
        /** @brief The type std::mt19937 expects as its seed. */
        using seed_type = std::mt19937::result_type;

        /** @brief How many positions one ship is offered before giving up. */
        static constexpr std::size_t max_ship_attempts = 200;
        /** @brief How many times the whole fleet is attempted from scratch. */
        static constexpr std::size_t max_fleet_attempts = 20;

        std::mt19937 gen_;   ///< The engine every random choice here comes from.

    public:

        /**
         * @brief Builds a strategy with a given or a fresh random seed.
         * @param seed the value the generator starts from; a random one by default.
         *
         * Pass an explicit seed and the placement repeats exactly, which is what
         * makes a failing game replayable and the tests deterministic. The
         * default argument is evaluated at the call site, so a caller who does
         * not care gets a different fleet every run without having to say so.
         */
        explicit AutomaticPlacementStrategy(seed_type seed = std::random_device{}()) : gen_(seed) {}

        /**
         * @brief Produces one complete arrangement of the fleet.
         * @return one Placement per ship, longest ship first, or nothing if no
         *         legal arrangement was found within the attempt limits.
         *
         * The fleet is walked in the order rules.hpp declares it, longest class
         * first. That order matters: the long ships are the hard ones to fit,
         * and placing them while the board is still empty is what keeps the
         * number of restarts small.
         */
        [[nodiscard]] PlacementResult make_placement() override
        {
            Grid2D<Busy> cells(board_size, board_size, Busy::Free);

            std::vector<Placement> placements;
            placements.reserve(ship_count);

            for (std::size_t attempt = 0; attempt < max_fleet_attempts; ++attempt)
            {
                // A fresh attempt starts from an empty board and an empty list.
                // Both objects are reused rather than rebuilt: the grid keeps
                // its single allocation and the vector keeps its capacity.
                cells.fill(Busy::Free);
                placements.clear();

                if (place_fleet(cells, placements))
                    return placements;
            }

            return std::nullopt;
        }

    private:

        /**
         * @brief Places every ship of the fleet once, in declaration order.
         * @param cells      scratch grid, modified in place.
         * @param placements receives one Placement per ship placed.
         * @return true if all of them found room.
         *
         * placed.value() rather than *placed. Both compile to the same
         * instructions once the "if" above has proved the optional is engaged,
         * but if that "if" is ever moved or removed, *placed is silent
         * undefined behaviour - measured: neither AddressSanitizer nor
         * UndefinedBehaviorSanitizer reports it, only a standard library built
         * with its own assertions enabled does. value() throws
         * std::bad_optional_access in every build.
         *
         * On failure it returns immediately and leaves both arguments in a
         * partially filled state. That is deliberate: the caller is going to
         * reset them anyway, and clearing them here would only hide from a
         * debugger how far the attempt got.
         */
        bool place_fleet(Grid2D<Busy>& cells, std::vector<Placement>& placements)
        {
            for (const ShipClass& ship_class : fleet)
            {
                for (std::size_t i = 0; i < ship_class.count; ++i)
                {
                    const std::optional<Placement> placed = place_one_ship(cells, ship_class.size);

                    if (!placed)
                        return false;

                    placements.push_back(placed.value());
                }
            }

            return true;
        }

        /**
         * @brief Finds room for one ship and reserves it.
         * @param cells scratch grid, modified in place on success.
         * @param len   length of the ship in cells.
         * @return where the ship went, or nothing after max_ship_attempts tries.
         *
         * The grid is only written to once a candidate has been accepted, so a
         * rejected candidate leaves no trace and the caller needs no rollback.
         */
        std::optional<Placement> place_one_ship(Grid2D<Busy>& cells, std::size_t len)
        {
            for (std::size_t attempt = 0; attempt < max_ship_attempts; ++attempt)
            {
                const Placement candidate = random_placement(len);

                if (!detail::is_area_free(cells, area_of(candidate)))
                    continue;

                detail::mark_with_ring(cells, candidate);
                return candidate;
            }

            return std::nullopt;
        }

        /**
         * @brief Draws one random position for a ship, ignoring other ships.
         * @param len length of the ship in cells.
         * @return a normalized placement that lies inside the board.
         *
         * Orientation first, then a start drawn from the range in which a ship
         * of this length still fits in that direction. Drawing the start from
         * the valid range rather than from the whole board and testing it
         * afterwards means this function cannot fail, so it returns a Placement
         * and not an optional, and the only rejection left in the algorithm is
         * the one that actually depends on the other ships.
         */
        Placement random_placement(std::size_t len)
        {
            assert(len >= min_ship_size && len <= max_ship_size);

            std::uniform_int_distribution<int> orientation_dist(0, 3);
            const Orientation dir = static_cast<Orientation>(orientation_dist(gen_));
            const detail::Step step = detail::step_of(dir);

            const detail::Range rows = detail::start_range(len, step.row);
            const detail::Range cols = detail::start_range(len, step.col);

            std::uniform_int_distribution<std::size_t> row_dist(rows.lo, rows.hi);
            std::uniform_int_distribution<std::size_t> col_dist(cols.lo, cols.hi);

            const Coords start{ row_dist(gen_), col_dist(gen_) };

            return detail::normalized(start, detail::far_end(start, len, step));
        }
    };

} // namespace seabattle
