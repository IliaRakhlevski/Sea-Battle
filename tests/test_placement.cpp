/**
 * @file test_placement.cpp
 * @brief The automatic placement strategy: every fleet it produces is legal,
 *        and the same seed produces the same fleet.
 *
 * Randomness is tested by its properties, never by exact coordinates: the
 * engine behind std::mt19937 is fixed by the standard, but
 * std::uniform_int_distribution is not, so the same seed gives different
 * fleets on different standard libraries.
 */

#include "check.hpp"
#include "fleet_helpers.hpp"

#include "seabattle/placement_strategy.hpp"

#include <vector>

using namespace seabattle;
using seabattle::test::cells_of;
using seabattle::test::length_of;

namespace {

    /** @brief Every rule a fleet must satisfy, checked from scratch. */
    void check_fleet_is_legal(const std::vector<Placement>& ships)
    {
        CHECK(ships.size() == ship_count);

        // lengths, class by class, in the order rules.hpp declares them
        std::size_t index = 0;

        for (const ShipClass& ship_class : fleet)
            for (std::size_t i = 0; i < ship_class.count && index < ships.size(); ++i, ++index)
            {
                const Placement& ship = ships[index];

                CHECK(in_board(ship.start) && in_board(ship.end));
                CHECK(ship.start.row <= ship.end.row && ship.start.col <= ship.end.col);
                CHECK(ship.start.row == ship.end.row || ship.start.col == ship.end.col);
                CHECK(length_of(ship) == ship_class.size);
            }

        // who owns each cell; -1 is water
        int owner[board_size][board_size];

        for (auto& row : owner)
            for (int& cell : row)
                cell = -1;

        for (std::size_t i = 0; i < ships.size(); ++i)
            for (const Coords& c : cells_of(ships[i]))
            {
                CHECK(owner[c.row][c.col] == -1);            // no overlap
                owner[c.row][c.col] = static_cast<int>(i);
            }

        // no two ships touch, not even diagonally
        for (std::size_t row = 0; row < board_size; ++row)
            for (std::size_t col = 0; col < board_size; ++col)
            {
                if (owner[row][col] < 0)
                    continue;

                const Rect around = ring_around(Placement{ { row, col }, { row, col } });

                for (std::size_t r = around.top_left.row; r <= around.bottom_right.row; ++r)
                    for (std::size_t c = around.top_left.col; c <= around.bottom_right.col; ++c)
                        CHECK(owner[r][c] < 0 || owner[r][c] == owner[row][col]);
            }
    }

    bool same_fleet(const std::vector<Placement>& a, const std::vector<Placement>& b)
    {
        if (a.size() != b.size())
            return false;

        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].start != b[i].start || a[i].end != b[i].end)
                return false;

        return true;
    }

} // namespace

int main()
{
    // --- every fleet is legal, and the strategy never runs out of attempts ---
    for (unsigned seed = 0; seed < 2000; ++seed)
    {
        AutomaticPlacementStrategy strategy(seed);
        const PlacementResult result = strategy.make_placement();

        CHECK(result.has_value());

        if (result)
            check_fleet_is_legal(*result);
    }

    // --- reproducibility ---------------------------------------------------
    {
        AutomaticPlacementStrategy a(12345);
        AutomaticPlacementStrategy b(12345);
        AutomaticPlacementStrategy c(12346);

        const auto fa = a.make_placement();
        const auto fb = b.make_placement();
        const auto fc = c.make_placement();

        CHECK(fa && fb && same_fleet(*fa, *fb));      // same seed, same fleet
        CHECK(fa && fc && !same_fleet(*fa, *fc));     // another seed, another fleet

        // the generator lives on between calls: two fleets from one object differ
        const auto second = a.make_placement();
        CHECK(second && !same_fleet(*fa, *second));
    }

    // --- the helpers that keep a draw on the board ---------------------------
    {
        using detail::start_range;

        // a four-decker growing right may start in columns 0..6
        CHECK(start_range(4, +1).lo == 0 && start_range(4, +1).hi == 6);
        // growing left, in columns 3..9
        CHECK(start_range(4, -1).lo == 3 && start_range(4, -1).hi == 9);
        // along the axis it does not grow on, anywhere
        CHECK(start_range(4, 0).lo == 0 && start_range(4, 0).hi == 9);
        // a one-decker fits everywhere whichever way it "grows"
        CHECK(start_range(1, +1).lo == 0 && start_range(1, +1).hi == 9);

        using detail::far_end;
        using detail::step_of;

        CHECK(far_end(Coords{ 4, 3 }, 4, step_of(Orientation::East)) == (Coords{ 4, 6 }));
        CHECK(far_end(Coords{ 4, 3 }, 4, step_of(Orientation::West)) == (Coords{ 4, 0 }));
        CHECK(far_end(Coords{ 3, 3 }, 4, step_of(Orientation::North)) == (Coords{ 0, 3 }));
        CHECK(far_end(Coords{ 6, 3 }, 4, step_of(Orientation::South)) == (Coords{ 9, 3 }));
    }

    return test::report("test_placement");
}
