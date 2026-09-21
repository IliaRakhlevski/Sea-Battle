#pragma once
/**
 * @file manual_targeting.hpp
 * @brief A targeting strategy that asks a person where to shoot.
 *
 * It lives here and not in the library for one reason: it reads from a stream,
 * and nothing in include/seabattle may know that input exists. The library
 * publishes the interface; whoever has a user interface brings the
 * implementation. This class is the proof that the arrangement works - an
 * outsider adds a strategy without a line of the engine changing.
 */

#include "seabattle/targeting_strategy.hpp"

#include <iosfwd>

namespace seabattle::console {

    /**
     * @brief Asks the player for a cell, and keeps asking until it makes sense.
     */
    class ManualTargeting : public ITargetingStrategy
    {
        std::istream& in_;    ///< Where the answer comes from.
        std::ostream& out_;   ///< Where the question goes.

    public:

        /**
         * @brief Builds a strategy that talks to the given streams.
         * @param in  where to read from; std::cin in the application.
         * @param out where to write to; std::cout in the application.
         *
         * Streams are parameters rather than std::cin and std::cout used
         * directly, so a test can play a whole game from a string.
         */
        ManualTargeting(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

        /**
         * @brief Asks where to shoot.
         * @param map what this player knows; used to reject a repeat.
         * @return a cell that is still Unknown on @p map.
         * @throws std::runtime_error if the input ends before an answer arrives.
         *
         * A cell that has already been fired at is rejected HERE, before the
         * shot is sent. The opponent's board would refuse it anyway, but that
         * refusal is a second line of defence: a player must not be able to
         * waste a turn on a question they can already answer from their own map.
         *
         * The interface promises a coordinate and has no way to say "no move",
         * which is deliberate - leaving the game is not a move. Input running
         * out is not a move either, and it is not an answer this function can
         * invent, so it throws.
         */
        [[nodiscard]] Coords next_target(const EnemyMap& map) override;
    };

} // namespace seabattle::console
