/**
 * @file test_enemy_map.cpp
 * @brief The knowledge map, played against real boards until the fleet is gone.
 *
 * Every shot is answered by a genuine OwnBoard, so the map is checked against
 * the truth rather than against expectations written by hand: Sunk only ever
 * lands on a real ship, Crossed only on real water, and a Hit cell always
 * belongs to a ship that is still afloat.
 */

#include "check.hpp"
#include "fleet_helpers.hpp"

#include "seabattle/enemy_map.hpp"
#include "seabattle/own_board.hpp"
#include "seabattle/placement_strategy.hpp"

#include <random>
#include <variant>
#include <vector>

using namespace seabattle;

namespace {

    bool is_ship(const OwnBoard& board, std::size_t row, std::size_t col)
    {
        return std::holds_alternative<ShipCell>(board.cells()(row, col));
    }

} // namespace

int main()
{
    std::mt19937 gen(2026);

    for (int game = 0; game < 1000; ++game)
    {
        AutomaticPlacementStrategy placement(gen());
        const PlacementResult fleet_list = placement.make_placement();
        CHECK(fleet_list.has_value());

        OwnBoard target;
        CHECK(fleet_list && target.deploy(*fleet_list));

        EnemyMap map;
        int shots = 0;

        while (map.sunk_ships() < ship_count && shots < 100)
        {
            // fire at random, but only where nothing is known yet
            std::vector<Coords> unknown;

            for (std::size_t r = 0; r < board_size; ++r)
                for (std::size_t c = 0; c < board_size; ++c)
                    if (map.cells()(r, c) == EnemyCellState::Unknown)
                        unknown.push_back(Coords{ r, c });

            CHECK(!unknown.empty());         // while ships remain, somewhere is left to shoot
            if (unknown.empty())
                break;

            std::uniform_int_distribution<std::size_t> pick(0, unknown.size() - 1);
            const Coords at = unknown[pick(gen)];

            const std::optional<ShotResult> answer = target.receive_shot(at);
            CHECK(answer.has_value());       // never asked twice, so never refused
            if (!answer)
                break;

            map.record(at, *answer);
            ++shots;

            // after every shot of the first games: a Hit cell is a ship still afloat
            if (game < 50)
                for (std::size_t r = 0; r < board_size; ++r)
                    for (std::size_t c = 0; c < board_size; ++c)
                        if (map.cells()(r, c) == EnemyCellState::Hit)
                            CHECK(is_ship(target, r, c) && !target.is_sunk_at(Coords{ r, c }));
        }

        CHECK(map.sunk_ships() == ship_count);
        CHECK(target.is_defeated());

        // the finished map against the truth
        for (std::size_t r = 0; r < board_size; ++r)
            for (std::size_t c = 0; c < board_size; ++c)
            {
                const EnemyCellState known = map.cells()(r, c);

                CHECK(known != EnemyCellState::Hit);                      // nothing left damaged
                if (is_ship(target, r, c))
                    CHECK(known == EnemyCellState::Sunk);                 // every deck accounted for
                else
                    CHECK(known != EnemyCellState::Sunk);                 // water is never "sunk"
                if (known == EnemyCellState::Crossed)
                    CHECK(!is_ship(target, r, c));                        // deduction never lies
            }
    }

    // --- Crossed is written over Unknown only: a miss stays a miss ------------
    {
        EnemyMap map;

        map.record(Coords{ 1, 0 }, ShotResult::Miss);   // next to the one-decker below
        map.record(Coords{ 0, 0 }, ShotResult::Sunk);   // a one-decker in the corner

        CHECK(map.cells()(0, 0) == EnemyCellState::Sunk);
        CHECK(map.cells()(1, 0) == EnemyCellState::Missed);   // the shot is still visible
        CHECK(map.cells()(0, 1) == EnemyCellState::Crossed);
        CHECK(map.cells()(1, 1) == EnemyCellState::Crossed);
        CHECK(map.cells()(2, 2) == EnemyCellState::Unknown);  // outside the ring
        CHECK(map.sunk_ships() == 1);
    }

    // --- a ship sunk by a shot in its middle is reconstructed whole ------------
    {
        EnemyMap map;

        map.record(Coords{ 5, 2 }, ShotResult::Hit);
        map.record(Coords{ 5, 4 }, ShotResult::Hit);
        map.record(Coords{ 5, 3 }, ShotResult::Sunk);   // the last cell is in the middle

        for (std::size_t c = 2; c <= 4; ++c)
            CHECK(map.cells()(5, c) == EnemyCellState::Sunk);

        CHECK(map.cells()(5, 1) == EnemyCellState::Crossed);
        CHECK(map.cells()(5, 5) == EnemyCellState::Crossed);
        CHECK(map.cells()(4, 3) == EnemyCellState::Crossed);
        CHECK(map.cells()(6, 3) == EnemyCellState::Crossed);
    }

    return test::report("test_enemy_map");
}
