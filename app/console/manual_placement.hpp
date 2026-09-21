#pragma once
/**
 * @file manual_placement.hpp
 * @brief A placement strategy that lets a person lay out their own fleet.
 *
 * Here rather than in the library for the same reason as manual targeting: it
 * reads from a stream.
 *
 * It checks every ship as it is entered instead of handing a broken list to
 * the board. The board would reject the list anyway - it never trusts what
 * arrives - but a rejection at that point has nothing useful to say: the fleet
 * is already finished and the player would have to start again. The rules are
 * therefore checked twice on purpose, and this is the copy whose job is to
 * explain what went wrong.
 */

#include "seabattle/placement_strategy.hpp"

#include <iosfwd>
#include <vector>

namespace seabattle::console {

    /**
     * @brief Asks the player for each ship in turn.
     */
    class ManualPlacement : public IPlacementStrategy
    {
        std::istream& in_;    ///< Where the answers come from.
        std::ostream& out_;   ///< Where the questions go.

    public:

        /**
         * @brief Builds a strategy that talks to the given streams.
         * @param in  where to read from; std::cin in the application.
         * @param out where to write to; std::cout in the application.
         */
        ManualPlacement(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

        /**
         * @brief Walks the player through the whole fleet.
         * @return the finished list, or nothing if the player gave up.
         *
         * Typing "random" at any prompt hands the rest of the job to the
         * automatic strategy - the fleet has ten ships and few people want to
         * place all of them before every game. Typing "quit" gives up, which
         * is the empty result the interface allows for.
         */
        [[nodiscard]] PlacementResult make_placement() override;

    private:

        /** @brief Draws what has been placed so far. */
        void draw(const std::vector<Placement>& placed);

        /**
         * @brief Whether a ship of this length may go here, given what is placed.
         * @param placed the ships already accepted.
         * @param ship   the candidate.
         * @param len    the length the candidate must have.
         * @param why    filled with the reason when the answer is false.
         */
        [[nodiscard]] static bool is_legal(const std::vector<Placement>& placed,
                                           const Placement& ship,
                                           std::size_t len,
                                           const char*& why);
    };

} // namespace seabattle::console
