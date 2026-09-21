// Tests for the own board.
//
// The list of checks is derived from the STRUCTURE of the problem, not from
// inspiration: every category in full, boring cases included. The expression
// evaluator taught this the hard way - "2+3*4" was never typed in because it
// looked too trivial to be worth the time, and that is exactly where the
// infinite loop was hiding.
//
// Categories that must be covered:
//   placement: inside the board, overlap, touching by a side, touching by a
//              corner, correct fleet composition, ship does not fit at an edge
//   shots:     miss, hit, last remaining deck (sunk), shooting the same cell twice
//   game end:  not over while one ship survives; over once all are sunk
//   drawing:   the board exposes what the UI needs and nothing more

#include "seabattle/own_board.hpp"

#include <cassert>
#include <iostream>

int main()
{
    // TODO(Ilia): tests are written right after the declarations,
    // before the implementation.
    std::cout << "own_board: no tests yet\n";
    return 0;
}
