#pragma once
/**
 * @file session.hpp
 * @brief The game itself: two players, the toss, the turns, the outcome.
 *
 * Design decisions:
 *
 * - Session OWNS both players. They point at each other, so neither may
 *   outlive the other, and deleting Player's move operations only stops the
 *   pointers being invalidated - it does nothing about lifetime. One object
 *   owning both, and outliving both, is what actually closes that hole.
 *
 * - Because Player cannot be moved, it cannot be handed over ready-made. So
 *   Session is given the four strategies and builds the players itself. They
 *   arrive in two PlayerSetup bundles rather than as four loose pointers:
 *   four arguments of two alternating types is a signature whose arguments
 *   will one day be swapped, and swapping the two PLAYERS' bundles would
 *   compile in silence.
 *
 * - Not one line of output happens here. Everything the outside world learns
 *   arrives through observers, and with none attached a game runs silently -
 *   which is what makes it possible to play twenty thousand of them in a test
 *   and measure how good a strategy is.
 *
 * - The observers get NOTIFICATIONS, not requests: an event travels one way
 *   and nothing is expected back. That is what an observer is for, and it is
 *   why this works here while it would not have worked for the shot itself,
 *   where an answer is the whole point.
 *
 * - Two players means two observers, one per side. A shot and its outcome are
 *   public knowledge - both sides learn them by the rules of the game - so a
 *   single stream of events gives nothing away. What differs between the two
 *   views is which boards each one draws, and each observer takes those from
 *   the player it serves.
 *
 * - Who moves first is decided by a toss, seeded from outside like everything
 *   else random in this project, so that a whole game can be replayed.
 */

#include "coord.hpp"
#include "placement_strategy.hpp"
#include "player.hpp"
#include "shot_result.hpp"
#include "targeting_strategy.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace seabattle {

    /** @brief Which of the two players is meant. */
    enum class Side
    {
        First,   ///< The player built from the first PlayerSetup.
        Second   ///< The player built from the second PlayerSetup.
    };

    /** @brief The other side. */
    [[nodiscard]] constexpr Side other(Side side) noexcept
    {
        return side == Side::First ? Side::Second : Side::First;
    }

    /**
     * @brief Everything that happens in a game, reported as it happens.
     *
     * Implemented outside the library - by the console, by a logger, by a test
     * that only counts moves. A session with no observers attached plays in
     * complete silence.
     */
    struct IGameObserver
    {
        virtual ~IGameObserver() = default;

        /**
         * @brief Both fleets are on their boards; the first shot has not been fired.
         *
         * Not pure: an observer that only writes a log has nothing to do here.
         * A console uses it to draw the opening position, which is the only
         * moment a player sees the board before anything has been shot at.
         */
        virtual void on_fleets_deployed() {}

        /**
         * @brief One shot has been fired and answered.
         * @param shooter who fired.
         * @param at      the cell fired at.
         * @param result  what it turned out to be.
         */
        virtual void on_shot(Side shooter, const Coords& at, ShotResult result) = 0;

        /**
         * @brief The game is over.
         * @param winner the player whose opponent has no ships left.
         */
        virtual void on_game_over(Side winner) = 0;
    };

    /**
     * @brief The two strategies one player is built from.
     *
     * A named bundle rather than two loose arguments: the manual and the
     * automatic strategies are created outside - the manual ones need a user
     * interface, which nothing in this library may know about - and handed
     * over here in one piece per player.
     */
    struct PlayerSetup
    {
        std::unique_ptr<IPlacementStrategy> placement;   ///< Where this player's fleet goes.
        std::unique_ptr<ITargetingStrategy> targeting;   ///< Where this player shoots.
    };

    /**
     * @brief One game between two players.
     *
     * @invariant both players exist for as long as the session does;
     * @invariant each player's opponent pointer refers to the other one.
     */
    class Session
    {
        /** @brief The type std::mt19937 expects as its seed. */
        using seed_type = std::mt19937::result_type;

        Player first_;    ///< Built from the first setup.
        Player second_;   ///< Built from the second setup.

        std::mt19937 gen_;   ///< Used for the toss and nothing else.

        std::vector<IGameObserver*> observers_;   ///< Not owned; they outlive the session.

    public:

        /**
         * @brief Builds both players and wires them to each other.
         * @param first     the first player's two strategies.
         * @param second    the second player's two strategies.
         * @param toss_seed decides who moves first; pass a fixed value to replay.
         *
         * The wiring happens here rather than later because both players are
         * members: once the constructor body runs they both exist, and their
         * addresses will not change for as long as the session does.
         */
        Session(PlayerSetup first, PlayerSetup second, seed_type toss_seed = std::random_device{}())
            : first_(std::move(first.placement), std::move(first.targeting)),
              second_(std::move(second.placement), std::move(second.targeting)),
              gen_(toss_seed)
        {
            first_.set_opponent(second_);
            second_.set_opponent(first_);
        }

        Session(const Session&)            = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&)                 = delete;
        Session& operator=(Session&&)      = delete;

        /**
         * @brief Adds something that wants to be told what happens.
         * @param observer lives at least as long as this session.
         *
         * Attach observers BEFORE run(): the opening position is announced
         * once, and an observer added afterwards has already missed it.
         */
        void add_observer(IGameObserver& observer)
        {
            observers_.push_back(&observer);
        }

        /**
         * @brief Plays one game from the toss to the last ship.
         * @return the winner, or nothing if a fleet could not be deployed.
         *
         * The rules of this variant: a hit earns another shot, a miss passes
         * the turn. That is the whole loop, and it is the reason Player::play_turn
         * fires ONE shot rather than taking a whole turn - deciding whether to
         * fire again belongs here.
         *
         * Deployment failing is an ordinary outcome, not an error: a random
         * strategy can run out of attempts, and a human can give up. Calling
         * run() twice on the same session also lands here, because a board
         * refuses to be deployed a second time.
         */
        [[nodiscard]] std::optional<Side> run()
        {
            if (!first_.deploy() || !second_.deploy())
                return std::nullopt;

            for (IGameObserver* observer : observers_)
                observer->on_fleets_deployed();

            std::uniform_int_distribution<int> toss(0, 1);
            Side current = (toss(gen_) == 0) ? Side::First : Side::Second;

            for (;;)
            {
                Player& shooter = player_at(current);

                const TurnResult turn = shooter.play_turn();

                for (IGameObserver* observer : observers_)
                    observer->on_shot(current, turn.at, turn.result);

                if (shooter.has_won())
                {
                    for (IGameObserver* observer : observers_)
                        observer->on_game_over(current);

                    return current;
                }

                if (turn.result == ShotResult::Miss)
                    current = other(current);
            }
        }

        /**
         * @brief One of the two players, for an observer to draw.
         * @param side which one.
         */
        [[nodiscard]] const Player& player(Side side) const noexcept
        {
            return side == Side::First ? first_ : second_;
        }

    private:

        /** @brief One of the two players, as something that can be played. */
        [[nodiscard]] Player& player_at(Side side) noexcept
        {
            return side == Side::First ? first_ : second_;
        }
    };

} // namespace seabattle
