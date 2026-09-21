/**
 * @file test_session.cpp
 * @brief Whole games between two computer players, watched by observers.
 *
 * Nothing here prints a board: a session with no user interface plays in
 * silence, which is the point. The observers only record what they are told,
 * and the record is then checked against the rules.
 */

#include "check.hpp"

#include "seabattle/session.hpp"

#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace seabattle;

namespace {

    /** @brief Writes down every event, one line each. */
    class Recorder : public IGameObserver
    {
    public:
        std::ostringstream log;
        int deployed = 0;
        int shots = 0;
        int finished = 0;

        void on_fleets_deployed() override
        {
            ++deployed;
            log << "start\n";
        }

        void on_shot(Side shooter, const Coords& at, ShotResult result) override
        {
            ++shots;
            log << (shooter == Side::First ? '1' : '2') << ' ' << at.row << ' ' << at.col << ' '
                << (result == ShotResult::Miss ? "miss" : result == ShotResult::Hit ? "hit" : "sunk") << '\n';
        }

        void on_game_over(Side winner) override
        {
            ++finished;
            log << "winner " << (winner == Side::First ? '1' : '2') << '\n';
        }
    };

    /** @brief Implements only what it must - the one optional event is left alone. */
    class Minimal : public IGameObserver
    {
    public:
        void on_shot(Side, const Coords&, ShotResult) override {}
        void on_game_over(Side) override {}
    };

    PlayerSetup computer(std::mt19937::result_type placement_seed, std::mt19937::result_type targeting_seed)
    {
        return PlayerSetup{ std::make_unique<AutomaticPlacementStrategy>(placement_seed),
                            std::make_unique<SimpleTargetingStrategy>(targeting_seed) };
    }

    std::string play_and_record(std::mt19937::result_type toss)
    {
        Session session(computer(21, 22), computer(23, 24), toss);
        Recorder recorder;
        session.add_observer(recorder);
        (void)session.run();
        return recorder.log.str();
    }

} // namespace

int main()
{
    // --- no observers at all: the game still runs, silently, to the end ------
    {
        Session session(computer(1, 2), computer(3, 4), 5);
        const std::optional<Side> winner = session.run();

        CHECK(winner.has_value());
        if (winner)
        {
            CHECK(session.player(*winner).has_won());
            CHECK(session.player(other(*winner)).is_defeated());
            CHECK(!session.player(*winner).is_defeated());
        }
    }

    // --- two observers are told the same story, start and end exactly once ---
    {
        Session session(computer(11, 12), computer(13, 14), 15);
        Recorder a;
        Recorder b;
        session.add_observer(a);
        session.add_observer(b);
        const std::optional<Side> winner = session.run();

        CHECK(winner.has_value());
        CHECK(a.deployed == 1 && b.deployed == 1);
        CHECK(a.finished == 1 && b.finished == 1);
        CHECK(a.shots > 0 && a.shots == b.shots);
        CHECK(a.log.str() == b.log.str());
    }

    // --- the log obeys the rules: a hit shoots again, a miss passes the turn ---
    {
        std::istringstream lines(play_and_record(35));
        std::string line;
        std::string previous_side;
        std::string previous_result;

        while (std::getline(lines, line))
        {
            if (line == "start" || line.rfind("winner", 0) == 0)
                continue;

            std::istringstream fields(line);
            std::string side;
            std::string row;
            std::string col;
            std::string result;
            fields >> side >> row >> col >> result;

            if (!previous_side.empty())
            {
                if (previous_result == "miss")
                    CHECK(side != previous_side);
                else
                    CHECK(side == previous_side);
            }

            previous_side = side;
            previous_result = result;
        }

        CHECK(previous_result == "sunk");     // a game can only end on a sinking
    }

    // --- the same seeds replay the same game, move for move -------------------
    CHECK(play_and_record(100) == play_and_record(100));

    // A different toss SEED need not change the game - the coin has two sides,
    // and two seeds often land the same way. What must hold is that the toss
    // decides something, so look for a seed that lands the other way.
    {
        bool toss_matters = false;

        for (std::mt19937::result_type seed = 101; seed < 130 && !toss_matters; ++seed)
            if (play_and_record(seed) != play_and_record(100))
                toss_matters = true;

        CHECK(toss_matters);
    }

    // --- the toss is fair -----------------------------------------------------
    {
        int first_started = 0;
        const int games = 400;

        for (int i = 0; i < games; ++i)
        {
            const auto s = static_cast<std::mt19937::result_type>(i);
            Session session(computer(4 * s + 1, 4 * s + 2), computer(4 * s + 3, 4 * s + 4), s);
            Recorder recorder;
            session.add_observer(recorder);
            (void)session.run();

            std::istringstream lines(recorder.log.str());
            std::string line;
            std::getline(lines, line);       // "start"
            std::getline(lines, line);       // the first shot
            if (!line.empty() && line[0] == '1')
                ++first_started;
        }

        CHECK(first_started > games * 40 / 100 && first_started < games * 60 / 100);
    }

    // --- an observer that implements only what it must is enough ---------------
    {
        Session session(computer(41, 42), computer(43, 44), 45);
        Minimal minimal;
        session.add_observer(minimal);
        CHECK(session.run().has_value());
    }

    // --- a session plays once; asking again is refused, not faked ------------
    {
        Session session(computer(51, 52), computer(53, 54), 55);
        CHECK(session.run().has_value());
        CHECK(!session.run().has_value());
    }

    return test::report("test_session");
}
