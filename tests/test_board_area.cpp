/**
 * @file test_board_area.cpp
 * @brief Bounds, and the ring around a ship - especially where it meets the edge.
 *
 * The ring is the one piece of geometry two unrelated parts depend on: the
 * placement strategy blocks it, the knowledge map crosses it out. The edges are
 * where the unsigned arithmetic has its mine, so they get most of the checks.
 */

#include "check.hpp"

#include "seabattle/board_area.hpp"

using namespace seabattle;

namespace {

    bool same(const Rect& r, Coords top_left, Coords bottom_right)
    {
        return r.top_left == top_left && r.bottom_right == bottom_right;
    }

} // namespace

int main()
{
    // --- in_board --------------------------------------------------------
    CHECK(in_board(Coords{ 0, 0 }));
    CHECK(in_board(Coords{ 9, 9 }));
    CHECK(in_board(Coords{ 0, 9 }));
    CHECK(!in_board(Coords{ 10, 0 }));
    CHECK(!in_board(Coords{ 0, 10 }));

    // A value that was negative before conversion is huge now, and fails the
    // same single comparison - there is no separate "negative" case to test.
    CHECK(!in_board(Coords{ static_cast<std::size_t>(-1), 0 }));

    // --- area_of ---------------------------------------------------------
    CHECK(same(area_of(Placement{ { 4, 3 }, { 4, 6 } }), { 4, 3 }, { 4, 6 }));

    // --- ring_around ------------------------------------------------------
    // in the middle: one cell wider on every side
    CHECK(same(ring_around(Placement{ { 4, 3 }, { 4, 6 } }), { 3, 2 }, { 5, 7 }));

    // top-left corner: clipped on two sides, no wraparound to 18446744073709551615
    CHECK(same(ring_around(Placement{ { 0, 0 }, { 2, 0 } }), { 0, 0 }, { 3, 1 }));

    // bottom-right corner: clipped on the other two sides
    CHECK(same(ring_around(Placement{ { 9, 9 }, { 9, 9 } }), { 8, 8 }, { 9, 9 }));

    // along the top edge, horizontal
    CHECK(same(ring_around(Placement{ { 0, 3 }, { 0, 6 } }), { 0, 2 }, { 1, 7 }));

    // along the right edge, vertical
    CHECK(same(ring_around(Placement{ { 3, 9 }, { 6, 9 } }), { 2, 8 }, { 7, 9 }));

    // a four-decker spanning the whole... not quite whole: 0..3 against the left edge
    CHECK(same(ring_around(Placement{ { 5, 0 }, { 5, 3 } }), { 4, 0 }, { 6, 4 }));

    return test::report("test_board_area");
}
