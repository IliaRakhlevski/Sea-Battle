#pragma once
/**
 * @file coord_format.hpp
 * @brief Turning a coordinate into text a person reads, and back.
 *
 * The third representation of a cell, and the only one a human ever sees:
 * "B5". The engine knows Coords{row, col}; a network channel would know bytes.
 * Neither of the two knows anything about letters, and this file is the only
 * place in the program that does.
 */

#include "seabattle/coord.hpp"

#include <optional>
#include <string>

namespace seabattle::console {

    /**
     * @brief The text a player would type for this cell.
     * @param cell a coordinate on the board.
     * @return the column as a letter and the row as a number, e.g. "B5".
     */
    [[nodiscard]] std::string to_text(const Coords& cell);

    /**
     * @brief Reads a cell from what a player typed.
     * @param text one cell, e.g. "B5", "b5" or " J10 ".
     * @return the coordinate, or nothing if the text is not a cell on this board.
     *
     * Case does not matter and surrounding spaces are ignored. Everything else
     * is rejected: a missing letter, a row out of range, trailing rubbish.
     */
    [[nodiscard]] std::optional<Coords> parse_cell(const std::string& text);

} // namespace seabattle::console
