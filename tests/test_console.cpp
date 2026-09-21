/**
 * @file test_console.cpp
 * @brief The console front end, tested without a console.
 *
 * Every console class takes its streams as constructor parameters rather than
 * using std::cin and std::cout directly. That is what makes this file possible:
 * a "player" here is a string of typed lines, and what was drawn is a string to
 * search.
 */

#include "check.hpp"
#include "fleet_helpers.hpp"

#include "console_view.hpp"
#include "coord_format.hpp"
#include "manual_placement.hpp"
#include "manual_targeting.hpp"

#include "seabattle/own_board.hpp"
#include "seabattle/session.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

using namespace seabattle;
using namespace seabattle::console;

namespace {

    bool contains(const std::string& text, const char* piece)
    {
        return text.find(piece) != std::string::npos;
    }

} // namespace

int main()
{
    // --- "B5" and back ----------------------------------------------------------
    for (std::size_t r = 0; r < board_size; ++r)
        for (std::size_t c = 0; c < board_size; ++c)
        {
            const std::optional<Coords> back = parse_cell(to_text(Coords{ r, c }));
            CHECK(back && *back == (Coords{ r, c }));
        }

    CHECK(to_text(Coords{ 0, 0 }) == "A1");
    CHECK(to_text(Coords{ 9, 9 }) == "J10");
    CHECK(parse_cell(" b5 ") == std::optional<Coords>(Coords{ 4, 1 }));   // case and spaces

    for (const char* rubbish : { "", " ", "K1", "A0", "A11", "A", "1A", "AA1", "B5x", "-1", "A 5" })
        CHECK(!parse_cell(rubbish).has_value());

    // --- manual targeting: rubbish, then a repeat, then a good cell ----------------
    {
        EnemyMap map;
        map.record(Coords{ 4, 1 }, ShotResult::Miss);      // B5 is already known

        std::istringstream typed("zz\nB5\nC7\n");
        std::ostringstream shown;
        ManualTargeting targeting(typed, shown);

        CHECK(targeting.next_target(map) == (Coords{ 6, 2 }));
        CHECK(contains(shown.str(), "Not a cell"));
        CHECK(contains(shown.str(), "already know"));
    }

    // --- manual targeting: input that ends is not an answer ----------------------
    {
        EnemyMap map;
        std::istringstream typed("");
        std::ostringstream shown;
        ManualTargeting targeting(typed, shown);

        bool threw = false;
        try
        {
            (void)targeting.next_target(map);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- manual placement: a legal fleet typed in, and the board accepts it -------
    {
        std::istringstream typed("y\nA1 A4\nC1 C3\nC5 C7\nE1 E2\nE4 E5\nE7 E8\nG1\nG3\nG5\nG7\n");
        std::ostringstream shown;
        ManualPlacement placement(typed, shown);
        const PlacementResult ships = placement.make_placement();

        CHECK(ships.has_value());
        OwnBoard board;
        CHECK(ships && board.deploy(*ships));
    }

    // --- manual placement: each mistake is named, and the player may try again ---
    {
        std::istringstream typed("y\n"
                                 "A1 B2\n"      // diagonal
                                 "A1 A3\n"      // wrong length for a four-decker
                                 "A1 A4\n"      // accepted
                                 "A6 A8\n"      // accepted
                                 "B6 B8\n"      // touches the one before
                                 "quit\n");
        std::ostringstream shown;
        ManualPlacement placement(typed, shown);

        CHECK(!placement.make_placement().has_value());
        CHECK(contains(shown.str(), "diagonally"));
        CHECK(contains(shown.str(), "not the length"));
        CHECK(contains(shown.str(), "touch"));
    }

    // --- manual placement: "n" and "random" hand the job to the automatic strategy -
    for (const char* answers : { "n\n", "y\nrandom\n" })
    {
        std::istringstream typed(answers);
        std::ostringstream shown;
        ManualPlacement placement(typed, shown);
        const PlacementResult ships = placement.make_placement();

        CHECK(ships && ships->size() == ship_count);
        OwnBoard board;
        CHECK(ships && board.deploy(*ships));
    }

    // --- the view: both boards, the legend, the shots, the outcome ----------------
    {
        PlayerSetup first{ std::make_unique<AutomaticPlacementStrategy>(1),
                           std::make_unique<SimpleTargetingStrategy>(2) };
        PlayerSetup second{ std::make_unique<AutomaticPlacementStrategy>(3),
                            std::make_unique<SimpleTargetingStrategy>(4) };
        Session session(std::move(first), std::move(second), 5);

        std::ostringstream shown;
        ConsoleView view(session, Side::First, shown);
        session.add_observer(view);
        CHECK(session.run().has_value());

        const std::string text = shown.str();
        CHECK(contains(text, "YOUR FLEET"));
        CHECK(contains(text, "ENEMY WATERS"));
        CHECK(contains(text, "A B C D E F G H I J"));
        CHECK(contains(text, "# your ship"));
        CHECK(contains(text, "* sunk"));
        CHECK(contains(text, "You fired at "));
        CHECK(contains(text, "The enemy fired at "));
        CHECK(contains(text, "You win.") || contains(text, "You lose."));
    }

    return test::report("test_console");
}
