#pragma once
/**
 * @file targeting_strategy.hpp
 * @brief How the next target is chosen: automatically or by a human.
 *
 * Design decisions:
 *
 * - This header includes enemy_map.hpp and NOT own_board.hpp. That is the fog
 *   of war expressed as a dependency: a targeting strategy cannot look at its
 *   own player's board, because that type is not even visible here. It sees
 *   only what shooting has revealed.
 *
 * - A human and an algorithm share the same interface. The engine only asks
 *   for a coordinate and does not care where it came from - a probability
 *   map, a random generator, the keyboard, or a recorded game.
 *
 * - Two implementations are planned so they can be compared: a simple one and
 *   one driven by a probability map. The comparison is a statistical test -
 *   average number of shots to win over many games - which is why a game must
 *   be playable with no user interface at all.
 *
 * - As with placement, randomness is seeded from outside so that a game can be
 *   replayed. Each strategy carries its own seed rather than deriving one from
 *   a single per-player number: two numbers to keep track of, but each strategy
 *   stays a self-contained object that can be built and tested on its own.
 *
 * - next_target() returns a plain Coords, not an optional. While any enemy ship
 *   is still afloat there is always an Unknown cell to fire at - measured over
 *   20000 games while testing EnemyMap, where the check never once failed - so
 *   an automatic strategy cannot run out of moves. A human leaving the game is
 *   not a move either; that is the user interface's business and never reaches
 *   this interface.
 */

#include "coord.hpp"
#include "enemy_map.hpp"
#include "rules.hpp"

#include <cassert>
#include <cstddef>
#include <random>
#include <vector>

namespace seabattle {

    /** @brief The interface every targeting strategy implements. */
    struct ITargetingStrategy
    {
        virtual ~ITargetingStrategy() = default;

        /**
         * @brief Chooses the next cell to fire at.
         * @param map everything the player knows about the opponent's board.
         * @return a cell that is still Unknown on @p map.
         * @pre at least one enemy ship is still afloat.
         *
         * The map arrives by const reference: a strategy reads it and decides,
         * and recording the outcome is somebody else's job. It could not write
         * to the map here even by accident.
         *
         * The method itself is NOT const. A strategy is allowed to remember
         * things between turns - which cell it tried last, where it is in a
         * sweep - in its own fields. Whatever it remembers, the map stays the
         * single source of truth about the board itself.
         */
        [[nodiscard]] virtual Coords next_target(const EnemyMap& map) = 0;
    };

    /**
     * @brief Fire blindly until something is struck, then finish it off.
     *
     * Two modes, and which one applies is read off the map rather than
     * remembered:
     *
     *   - no Hit cell anywhere -> nothing is damaged, pick at random among the
     *     cells still Unknown;
     *   - exactly one Hit cell -> a ship has been struck but its direction is
     *     unknown, try one of that cell's four neighbours;
     *   - two or more Hit cells -> they lie in a line, so the direction is
     *     known; step one cell beyond either end.
     *
     * The class therefore holds NO state about the game at all. Everything the
     * three cases need is already on the map: which shots missed (Missed),
     * where the damaged ship is (Hit), what has already gone down (Sunk). A
     * remembered "last shot" would be a second copy of a fact the map already
     * carries, with its own chances of disagreeing.
     *
     * At most one ship is ever damaged at a time, which is what makes "the Hit
     * cells" unambiguous. Damaging a second one would mean firing at a cell
     * next to the first, and ships may not touch - so the cells next to a
     * damaged ship belong either to that ship or to water.
     *
     * The generator is the one thing that does survive between calls, and it is
     * not game state: without it two consecutive turns would pick the same cell.
     * Its seed comes from outside so a game can be replayed exactly.
     */
    class SimpleTargetingStrategy : public ITargetingStrategy
    {
        /** @brief The type std::mt19937 expects as its seed. */
        using seed_type = std::mt19937::result_type;

        std::mt19937 gen_;   ///< The only field, and the only thing remembered.

    public:

        /**
         * @brief Builds a strategy with a given or a fresh random seed.
         * @param seed the value the generator starts from; a random one by default.
         */
        explicit SimpleTargetingStrategy(seed_type seed = std::random_device{}())
            : gen_(seed) {}

        /**
         * @brief Chooses the next cell to fire at.
         * @param map everything the player knows about the opponent's board.
         * @return a cell that is still Unknown on @p map.
         * @pre at least one enemy ship is still afloat.
         */
        [[nodiscard]] Coords next_target(const EnemyMap& map) override
        {
            const std::vector<Coords> damaged = cells_in_state(map, EnemyCellState::Hit);

            if (damaged.empty())
                return pick(cells_in_state(map, EnemyCellState::Unknown));

            return pick(finishing_shots(map, damaged));
        }

    private:

        /**
         * @brief Every cell of the map currently in the given state.
         * @param map   the map to scan.
         * @param state the state to collect.
         */
        [[nodiscard]] static std::vector<Coords> cells_in_state(const EnemyMap& map,
                                                                EnemyCellState state)
        {
            std::vector<Coords> found;

            for (std::size_t row = 0; row < board_size; ++row)
                for (std::size_t col = 0; col < board_size; ++col)
                    if (map.cells()(row, col) == state)
                        found.push_back(Coords{ row, col });

            return found;
        }

        /**
         * @brief Where the damaged ship can be finished off.
         * @param map     the map to read.
         * @param damaged the Hit cells, all belonging to one ship.
         * @return the Unknown cells that could hold the rest of that ship.
         *
         * With one Hit cell the direction is unknown, so all four neighbours
         * are candidates. With two or more the cells lie in a line and only
         * its two ends can continue it - shooting sideways there would be a
         * wasted shot, since a neighbour of a ship is water by the rules.
         *
         * The result is never empty: a ship that still has Hit cells is still
         * afloat, so at least one of its cells has not been fired at, and that
         * cell continues the line.
         */
        [[nodiscard]] static std::vector<Coords> finishing_shots(const EnemyMap& map,
                                                                 const std::vector<Coords>& damaged)
        {
            std::vector<Coords> candidates;

            if (damaged.size() == 1)
            {
                add_if_unknown(map, candidates, damaged.front(), -1, 0);
                add_if_unknown(map, candidates, damaged.front(), +1, 0);
                add_if_unknown(map, candidates, damaged.front(), 0, -1);
                add_if_unknown(map, candidates, damaged.front(), 0, +1);
            }
            else
            {
                Coords first = damaged.front();
                Coords last = damaged.front();

                for (const Coords& cell : damaged)
                {
                    if (cell.row < first.row || cell.col < first.col) first = cell;
                    if (cell.row > last.row || cell.col > last.col)   last = cell;
                }

                // One of the two differences is zero: the ship is a straight
                // line. That same zero makes the step below point along it.
                const bool vertical = (first.row != last.row);

                assert(vertical || first.col != last.col);

                add_if_unknown(map, candidates, first, vertical ? -1 : 0, vertical ? 0 : -1);
                add_if_unknown(map, candidates, last, vertical ? +1 : 0, vertical ? 0 : +1);
            }

            assert(!candidates.empty());

            return candidates;
        }

        /**
         * @brief Adds a neighbouring cell to @p out if it is on the board and Unknown.
         * @param map  the map to read.
         * @param out  the list being built.
         * @param from the cell to step away from.
         * @param drow row step: -1, 0 or +1.
         * @param dcol column step: -1, 0 or +1.
         *
         * The bound is tested BEFORE the step. The coordinates are unsigned, so
         * "row - 1" at row 0 is not -1 but 18446744073709551615, and that
         * wraparound is defined behaviour no sanitizer would report.
         */
        static void add_if_unknown(const EnemyMap& map, std::vector<Coords>& out,
                                   const Coords& from, int drow, int dcol)
        {
            if (drow < 0 && from.row == 0)              return;
            if (dcol < 0 && from.col == 0)              return;
            if (drow > 0 && from.row + 1 >= board_size) return;
            if (dcol > 0 && from.col + 1 >= board_size) return;

            const Coords next{ from.row + static_cast<std::size_t>(drow),
                               from.col + static_cast<std::size_t>(dcol) };

            if (map.cells()(next.row, next.col) == EnemyCellState::Unknown)
                out.push_back(next);
        }

        /**
         * @brief One of the candidates, chosen uniformly.
         * @param candidates a non-empty list.
         */
        [[nodiscard]] Coords pick(const std::vector<Coords>& candidates)
        {
            assert(!candidates.empty());

            std::uniform_int_distribution<std::size_t> which(0, candidates.size() - 1);

            return candidates[which(gen_)];
        }
    };

} // namespace seabattle
