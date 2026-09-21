/**
 * @file main.cpp
 * @brief The console application: the outermost layer, and the only one that
 *        talks to a person.
 *
 * Everything that knows about a screen or a keyboard lives in this directory.
 * The library under include/seabattle has no idea either exists - which is what
 * makes it possible to play twenty thousand games in a test without printing a
 * character.
 *
 * All this file does is decide who the two players are, hand each of them its
 * two strategies, attach a view, and start the session. It does not draw, it
 * does not read, and it does not know a rule of the game.
 */

#include "console_view.hpp"
#include "manual_placement.hpp"
#include "manual_targeting.hpp"

#include "seabattle/placement_strategy.hpp"
#include "seabattle/session.hpp"
#include "seabattle/targeting_strategy.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <utility>

int main()
{
    using namespace seabattle;
    using namespace seabattle::console;

    std::cout << "SEA BATTLE\n"
              << "Russian rules: 10x10, ships 4-3-3-2-2-2-1-1-1-1, no touching,\n"
              << "a hit earns another shot, the first move is decided by a toss.\n";

    // The human is the first player, the computer the second. Each gets a
    // placement strategy and a targeting strategy; neither player can tell
    // which kind it was handed.
    PlayerSetup human{ std::make_unique<ManualPlacement>(std::cin, std::cout),
                       std::make_unique<ManualTargeting>(std::cin, std::cout) };

    PlayerSetup computer{ std::make_unique<AutomaticPlacementStrategy>(),
                          std::make_unique<SimpleTargetingStrategy>() };

    Session session(std::move(human), std::move(computer));

    // One view, looking through the human's eyes. The computer gets none, and
    // that is the whole difference between the two sides here.
    ConsoleView view(session, Side::First, std::cout);
    session.add_observer(view);

    try
    {
        const std::optional<Side> winner = session.run();

        if (!winner)
        {
            std::cout << "\nThe game did not start: no fleet was laid out.\n";
            return 1;
        }
    }
    catch (const std::exception& error)
    {
        std::cout << "\nStopped: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
