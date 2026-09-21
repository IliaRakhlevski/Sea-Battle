#pragma once
/**
 * @file console_view.hpp
 * @brief What one player sees: two boards side by side, redrawn as the game goes.
 *
 * An observer, and nothing more. It is told that something happened and reads
 * the current state from the player it serves; the event itself carries no
 * board data. Two players mean two of these, each looking through its own
 * player's eyes.
 *
 * Attaching none is a supported way to play: a session with no observers runs
 * in complete silence, which is how thousands of games get measured in tests.
 */

#include "seabattle/session.hpp"

#include <iosfwd>

namespace seabattle::console {

    /**
     * @brief Draws the game for one of the two players.
     *
     * @invariant only ever reads from the session; it changes nothing.
     */
    class ConsoleView : public IGameObserver
    {
        const Session& session_;   ///< Where the boards are read from.
        Side side_;                ///< Whose eyes we are looking through.
        std::ostream& out_;        ///< Where the picture goes.

    public:

        /**
         * @brief Builds a view over one side of a session.
         * @param session the game being watched; outlives this view.
         * @param side    which player this view belongs to.
         * @param out     where to draw; std::cout in the application, a string
         *                stream in a test.
         *
         * The stream is a parameter rather than std::cout written directly, so
         * that what this class produces can be checked by a test instead of
         * being looked at.
         */
        ConsoleView(const Session& session, Side side, std::ostream& out)
            : session_(session), side_(side), out_(out) {}

        /** @brief Draws the opening position, before a single shot. */
        void on_fleets_deployed() override;

        /**
         * @brief Redraws both boards and reports the shot underneath them.
         * @param shooter who fired.
         * @param at      where.
         * @param result  what came of it.
         */
        void on_shot(Side shooter, const Coords& at, ShotResult result) override;

        /**
         * @brief Announces the outcome.
         * @param winner the player whose opponent has no ships left.
         */
        void on_game_over(Side winner) override;

    private:

        /** @brief The two boards, side by side, with their headers. */
        void draw_boards();

        /** @brief The column letters above a board. */
        void draw_column_header();
    };

} // namespace seabattle::console
