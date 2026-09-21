/**
 * @file test_own_board.cpp
 * @brief The board a player defends: what it accepts as a fleet, and how it
 *        answers shots.
 *
 * deploy() must refuse every kind of illegal fleet a person could type, and a
 * refusal must leave the board untouched. receive_shot() must give the right
 * outcome, and refuse a cell it has already answered.
 */

#include "check.hpp"
#include "fleet_helpers.hpp"

#include "seabattle/own_board.hpp"

#include <vector>

using namespace seabattle;
using seabattle::test::legal_fleet;

namespace {

    /** @brief deploy() refused, and the board is still empty. */
    bool refused(std::vector<Placement> ships)
    {
        OwnBoard board;
        const bool accepted = board.deploy(ships);
        return !accepted && !board.is_deployed();
    }

} // namespace

int main()
{
    // --- a legal fleet ------------------------------------------------------
    {
        OwnBoard board;

        CHECK(!board.is_deployed());
        CHECK(!board.is_defeated());          // no fleet yet is not the same as lost
        CHECK(board.deploy(legal_fleet()));
        CHECK(board.is_deployed());
        CHECK(!board.deploy(legal_fleet()));  // a fleet is deployed once
    }

    // --- every kind of illegal fleet is refused, and the board stays empty ---
    {
        auto nine = legal_fleet();     nine.pop_back();
        CHECK(refused(nine));

        auto eleven = legal_fleet();   eleven.push_back(Placement{ { 8, 8 }, { 8, 8 } });
        CHECK(refused(eleven));

        auto stretched = legal_fleet(); stretched[0].end.col += 1;         // a five-decker
        CHECK(refused(stretched));

        auto diagonal = legal_fleet(); diagonal[0] = Placement{ { 0, 0 }, { 3, 3 } };
        CHECK(refused(diagonal));

        auto reversed = legal_fleet(); reversed[0] = Placement{ { 0, 3 }, { 0, 0 } };
        CHECK(refused(reversed));

        auto off_board = legal_fleet(); off_board[0] = Placement{ { 9, 7 }, { 9, 10 } };
        CHECK(refused(off_board));

        auto overlap = legal_fleet();  overlap[9] = overlap[8];
        CHECK(refused(overlap));

        // one cell diagonally away from the end of the four-decker (0,0)-(0,3)
        auto corner = legal_fleet();   corner[9] = Placement{ { 1, 4 }, { 1, 4 } };
        CHECK(refused(corner));

        // right next to it
        auto side = legal_fleet();     side[9] = Placement{ { 0, 4 }, { 0, 4 } };
        CHECK(refused(side));

        // ten ships, all legal on their own, but the wrong mix: two four-deckers
        auto wrong_mix = legal_fleet(); wrong_mix[1] = Placement{ { 8, 0 }, { 8, 3 } };
        CHECK(refused(wrong_mix));
    }

    // --- shots -----------------------------------------------------------------
    {
        OwnBoard board;
        CHECK(board.deploy(legal_fleet()));

        // water
        CHECK(board.receive_shot(Coords{ 9, 9 }) == ShotResult::Miss);
        CHECK(!board.receive_shot(Coords{ 9, 9 }).has_value());       // asked twice: refused

        // off the board
        CHECK(!board.receive_shot(Coords{ 10, 0 }).has_value());

        // the four-decker at (0,0)-(0,3)
        CHECK(board.receive_shot(Coords{ 0, 0 }) == ShotResult::Hit);
        CHECK(!board.is_sunk_at(Coords{ 0, 0 }));                     // damaged, not sunk
        CHECK(!board.receive_shot(Coords{ 0, 0 }).has_value());       // same deck again
        CHECK(board.receive_shot(Coords{ 0, 1 }) == ShotResult::Hit);
        CHECK(board.receive_shot(Coords{ 0, 2 }) == ShotResult::Hit);
        CHECK(board.receive_shot(Coords{ 0, 3 }) == ShotResult::Sunk);
        CHECK(board.is_sunk_at(Coords{ 0, 0 }) && board.is_sunk_at(Coords{ 0, 3 }));
        CHECK(!board.is_sunk_at(Coords{ 9, 9 }));                     // water never is
        CHECK(board.sunk_ships() == 1);

        // a one-decker goes down in one shot
        CHECK(board.receive_shot(Coords{ 6, 0 }) == ShotResult::Sunk);
        CHECK(board.sunk_ships() == 2);
        CHECK(!board.is_defeated());
    }

    // --- sinking everything ---------------------------------------------------
    {
        OwnBoard board;
        CHECK(board.deploy(legal_fleet()));

        std::size_t sunk_answers = 0;

        for (const Placement& ship : legal_fleet())
            for (const Coords& cell : test::cells_of(ship))
                if (board.receive_shot(cell) == ShotResult::Sunk)
                    ++sunk_answers;

        CHECK(sunk_answers == ship_count);    // exactly one "sunk" per ship
        CHECK(board.sunk_ships() == ship_count);
        CHECK(board.is_defeated());
    }

    return test::report("test_own_board");
}
