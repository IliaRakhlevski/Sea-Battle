#pragma once
/**
 * @file player.hpp
 * @brief A player: owns its board and its knowledge map, and runs its half of
 *        the game.
 *
 * Knows nothing about the user interface and nothing about humans. It knows
 * its two strategies and something to shoot at.
 *
 * Design decisions:
 *
 * - The opponent is held as an IShootable, not as a Player. That is the whole
 *   reason the interface exists: a player can be exercised with a stand-in
 *   that answers whatever a test needs, and an opponent in another process
 *   looks exactly the same from here. See shootable.hpp.
 *
 * - It is a POINTER, not a reference. Two players are wired to each other, and
 *   that cannot be done in either constructor - neither exists yet when the
 *   other is being built. A reference cannot be set later, so the link is made
 *   in a second step, and until it is made the pointer is null.
 *
 * - A plain pointer, not a shared_ptr. Session owns the players and outlives
 *   them both; there is no shared ownership to express, and two shared_ptrs
 *   pointing at each other would form a cycle that is never destroyed.
 *
 * - Copying and moving are deleted, and not only because unique_ptr members
 *   would have made copying impossible anyway. Once two players point at each
 *   other, moving one leaves the other holding the old address. Deleting the
 *   move turns that into a compile error - std::vector<Player> stops compiling
 *   at push_back, which is exactly where the bug would have been born. What it
 *   does NOT protect against is lifetime: a player destroyed while its
 *   opponent still points at it is a dangling pointer no compiler will
 *   mention. That is why Session owns both.
 *
 * - Both strategies arrive as unique_ptr BY VALUE. The transfer of ownership
 *   is then written in the type: the caller cannot pass one without saying
 *   std::move, and afterwards nobody is left wondering who deletes them.
 *
 * - deploy() is a separate step rather than part of the constructor. Placement
 *   can fail - a random strategy may run out of attempts, a human may give up -
 *   and from a constructor the only way to report that is an exception. As a
 *   separate call it is an ordinary bool that Session can act on.
 */

#include "enemy_map.hpp"
#include "own_board.hpp"
#include "placement_strategy.hpp"
#include "shootable.hpp"
#include "shot_result.hpp"
#include "targeting_strategy.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <utility>

namespace seabattle {

    /** @brief What one shot turned out to be, and where it went. */
    struct TurnResult
    {
        Coords     at;       ///< The cell that was fired at.
        ShotResult result;   ///< What the opponent answered.
    };

    /**
     * @brief One side of a game.
     *
     * @invariant the knowledge map and the opponent's board never disagree:
     *            every cell recorded here was answered by that board;
     * @invariant a player that has started playing has a fleet deployed and an
     *            opponent set.
     */
    class Player : public IShootable
    {
        OwnBoard own_board_;   ///< Our ships and the damage done to them.
        EnemyMap enemy_map_;   ///< What we have learned about the opponent.

        std::unique_ptr<IPlacementStrategy> placement_;   ///< Decides where our fleet goes.
        std::unique_ptr<ITargetingStrategy> targeting_;   ///< Decides where to shoot next.

        IShootable* opponent_ = nullptr;   ///< Set in a second step; see set_opponent().

    public:

        /**
         * @brief Builds a player that has neither a fleet nor an opponent yet.
         * @param placement how this player will lay out its ships.
         * @param targeting how this player will choose its shots.
         *
         * Both strategies are created outside - the manual ones need a user
         * interface, which a player must know nothing about - and handed over
         * here.
         */
        Player(std::unique_ptr<IPlacementStrategy> placement,
               std::unique_ptr<ITargetingStrategy> targeting)
            : placement_(std::move(placement)),
              targeting_(std::move(targeting))
        {
            assert(placement_ != nullptr);
            assert(targeting_ != nullptr);
        }

        Player(const Player&)            = delete;
        Player& operator=(const Player&) = delete;
        Player(Player&&)                 = delete;
        Player& operator=(Player&&)      = delete;

        /**
         * @brief Names what this player shoots at.
         * @param opponent normally the other player; in a test, a stand-in.
         *
         * Separate from the constructor because two players are built before
         * either can be told about the other.
         */
        void set_opponent(IShootable& opponent) noexcept
        {
            opponent_ = &opponent;
        }

        /**
         * @brief Asks the placement strategy for a fleet and puts it on the board.
         * @return false if the strategy produced nothing, or produced a list the
         *         board refused.
         *
         * The board validates the list even though the strategy has just built
         * it. The strategy may be a human, or code written by somebody else.
         */
        [[nodiscard]] bool deploy()
        {
            const PlacementResult placements = placement_->make_placement();

            if (!placements)
                return false;

            return own_board_.deploy(placements.value());
        }

        /**
         * @brief Fires one shot at the opponent and records the answer.
         * @return where the shot went and what it turned out to be.
         * @pre an opponent is set, a fleet is deployed, and the opponent still
         *      has a ship afloat.
         *
         * One shot, not one turn. A hit earns another shot under these rules,
         * and deciding whether to fire again belongs to whoever runs the game -
         * a player that looped here would never hand control back.
         *
         * The opponent refusing the shot is treated as a bug on THIS side: the
         * targeting strategy is given the knowledge map precisely so that it
         * never picks a cell that has already been resolved.
         *
         * The coordinate comes back with the outcome rather than being kept in
         * a field. Session needs it to tell the observers where the shot went,
         * and returning it costs nothing; a "last shot" member would be the
         * same fact stored a second time, with its own chance of going stale.
         */
        [[nodiscard]] TurnResult play_turn()
        {
            assert(opponent_ != nullptr);
            assert(own_board_.is_deployed());

            const Coords at = targeting_->next_target(enemy_map_);

            assert(enemy_map_.cells()(at.row, at.col) == EnemyCellState::Unknown);

            const std::optional<ShotResult> answer = opponent_->receive_shot(at);

            assert(answer.has_value());

            enemy_map_.record(at, answer.value());

            return TurnResult{ at, answer.value() };
        }

        /**
         * @brief Takes one shot from the opponent.
         * @param at the cell being fired at.
         * @return what happened, or nothing if the board refuses the shot.
         *
         * Pure forwarding: the board owns this decision, and a player has
         * nothing to add to it.
         */
        [[nodiscard]] std::optional<ShotResult> receive_shot(const Coords& at) override
        {
            return own_board_.receive_shot(at);
        }

        /** @brief Whether this player's whole fleet has been sunk. */
        [[nodiscard]] bool is_defeated() const
        {
            return own_board_.is_defeated();
        }

        /**
         * @brief Whether this player has sunk the opponent's whole fleet.
         *
         * Derived from our own knowledge map - nobody sends a "you won"
         * message, because a result both sides can work out cannot then be
         * forgotten to be sent.
         */
        [[nodiscard]] bool has_won() const
        {
            return enemy_map_.sunk_ships() == ship_count;
        }

        /** @brief Our own board, for drawing. */
        [[nodiscard]] const OwnBoard& own_board() const noexcept { return own_board_; }

        /** @brief What we know about the opponent, for drawing. */
        [[nodiscard]] const EnemyMap& enemy_map() const noexcept { return enemy_map_; }
    };

} // namespace seabattle
