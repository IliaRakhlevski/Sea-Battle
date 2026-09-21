#pragma once
/**
 * @file shootable.hpp
 * @brief Something that can be fired at and answers what happened.
 *
 * Design decisions:
 *
 * - This is the whole protocol between two players, in one method. The shot
 *   travels as the argument, the answer comes back as the return value of the
 *   same call - there is no second interface for "receiving the reply",
 *   because the shooter is the one who made the call and the answer lands in
 *   its own stack frame.
 *
 * - A player holds a pointer to THIS, not to another Player. Two consequences,
 *   and both were the point of introducing it:
 *
 *   1. A player can be exercised on its own. A test substitutes a few lines
 *      that answer whatever the case needs, and one turn is checked without a
 *      second board, a second fleet and two more strategies existing at all.
 *
 *   2. The opponent no longer has to live in this process. An implementation
 *      that sends the coordinate over a socket, waits, and returns what came
 *      back looks identical from here. The waiting is real there - the peer is
 *      a separate process - while locally the same call is an ordinary nested
 *      function call. Neither the shooter nor its strategies can tell.
 *
 * - An empty result is a REFUSAL, not an outcome: the cell had already been
 *   resolved, or it is not on the board. See shot_result.hpp for why that does
 *   not belong among the three outcomes, and why it is a second line of
 *   defence - the shooter is expected to never send such a shot in the first
 *   place.
 */

#include "coord.hpp"
#include "shot_result.hpp"

#include <optional>

namespace seabattle {

    /** @brief The receiving end of one shot. */
    struct IShootable
    {
        virtual ~IShootable() = default;

        /**
         * @brief Takes one shot and reports what it did.
         * @param at the cell being fired at.
         * @return the outcome, or nothing if the shot is refused.
         */
        [[nodiscard]] virtual std::optional<ShotResult> receive_shot(const Coords& at) = 0;
    };

} // namespace seabattle
