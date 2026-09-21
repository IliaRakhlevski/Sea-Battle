#include "manual_placement.hpp"

#include "coord_format.hpp"

#include "seabattle/board_area.hpp"
#include "seabattle/grid2d.hpp"
#include "seabattle/rules.hpp"

#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace seabattle::console {

    namespace {

        /** @brief The same text in lower case, with the ends trimmed. */
        std::string normalized(const std::string& text)
        {
            const std::size_t first = text.find_first_not_of(" \t");

            if (first == std::string::npos)
                return {};

            const std::size_t last = text.find_last_not_of(" \t");

            std::string result = text.substr(first, last - first + 1);

            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            return result;
        }

        /** @brief Length of a placement whose ends share a row or a column. */
        std::size_t length_of(const Placement& ship)
        {
            const std::size_t rows = (ship.start.row > ship.end.row ? ship.start.row - ship.end.row
                                                                    : ship.end.row - ship.start.row);
            const std::size_t cols = (ship.start.col > ship.end.col ? ship.start.col - ship.end.col
                                                                    : ship.end.col - ship.start.col);

            return rows + cols + 1;
        }

        /** @brief The same two ends, ordered so that start comes first on both axes. */
        Placement ordered(const Coords& a, const Coords& b)
        {
            return Placement{ Coords{ std::min(a.row, b.row), std::min(a.col, b.col) },
                              Coords{ std::max(a.row, b.row), std::max(a.col, b.col) } };
        }

    } // namespace

    bool ManualPlacement::is_legal(const std::vector<Placement>& placed,
                                   const Placement& ship,
                                   std::size_t len,
                                   const char*& why)
    {
        if (ship.start.row != ship.end.row && ship.start.col != ship.end.col)
        {
            why = "a ship runs in a straight line, not diagonally";
            return false;
        }

        if (length_of(ship) != len)
        {
            why = "that is not the length of the ship being placed";
            return false;
        }

        // Occupied cells are marked together with the ring around them, so a
        // candidate only has to check its own cells. That is the touching rule
        // turned into a property of the grid - the same trick the automatic
        // strategy uses.
        Grid2D<Busy> taken(board_size, board_size, Busy::Free);

        for (const Placement& other_ship : placed)
        {
            const Rect ring = ring_around(other_ship);

            for (std::size_t row = ring.top_left.row; row <= ring.bottom_right.row; ++row)
                for (std::size_t col = ring.top_left.col; col <= ring.bottom_right.col; ++col)
                    taken(row, col) = Busy::Taken;
        }

        const Rect area = area_of(ship);

        for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
            for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                if (taken(row, col) != Busy::Free)
                {
                    why = "ships may not overlap or touch, not even at a corner";
                    return false;
                }

        return true;
    }

    void ManualPlacement::draw(const std::vector<Placement>& placed)
    {
        Grid2D<char> picture(board_size, board_size, '.');

        for (const Placement& ship : placed)
        {
            const Rect area = area_of(ship);

            for (std::size_t row = area.top_left.row; row <= area.bottom_right.row; ++row)
                for (std::size_t col = area.top_left.col; col <= area.bottom_right.col; ++col)
                    picture(row, col) = '#';
        }

        out_ << "\n   ";

        for (std::size_t col = 0; col < board_size; ++col)
            out_ << ' ' << static_cast<char>('A' + static_cast<int>(col));

        out_ << '\n';

        for (std::size_t row = 0; row < board_size; ++row)
        {
            out_ << (row + 1 < 10 ? " " : "") << (row + 1) << " ";

            for (std::size_t col = 0; col < board_size; ++col)
                out_ << ' ' << picture(row, col);

            out_ << '\n';
        }

        out_ << '\n';
    }

    PlacementResult ManualPlacement::make_placement()
    {
        std::vector<Placement> placed;
        placed.reserve(ship_count);

        // Asked before anything else: ten ships is a long dialogue, and most of
        // the time a player just wants to start shooting.
        for (;;)
        {
            out_ << "\nPlace your ships yourself? [y/n]: ";

            std::string line;

            if (!std::getline(in_, line))
                return std::nullopt;

            const std::string answer = normalized(line);

            if (answer == "n" || answer == "no")
            {
                out_ << "Placing your fleet at random.\n";

                AutomaticPlacementStrategy automatic;

                return automatic.make_placement();
            }

            if (answer == "quit")
                return std::nullopt;

            if (answer == "y" || answer == "yes")
                break;

            out_ << "  Answer y or n.\n";
        }

        out_ << "\nFor each ship give its two end cells, e.g. \"A1 A4\".\n"
             << "Type \"random\" to have the whole fleet placed for you, or \"quit\" to give up.\n";

        for (const ShipClass& ship_class : fleet)
        {
            for (std::size_t i = 0; i < ship_class.count; ++i)
            {
                draw(placed);

                for (;;)
                {
                    out_ << "Ship of " << ship_class.size << " cell"
                         << (ship_class.size == 1 ? "" : "s") << ": ";

                    std::string line;

                    if (!std::getline(in_, line))
                        return std::nullopt;

                    const std::string command = normalized(line);

                    if (command == "quit")
                        return std::nullopt;

                    if (command == "random")
                    {
                        // The whole fleet, not the remainder: an arrangement is
                        // only legal as a whole, and finishing a half-placed one
                        // is a different and much harder problem.
                        out_ << "Placing the whole fleet at random.\n";

                        AutomaticPlacementStrategy automatic;

                        return automatic.make_placement();
                    }

                    std::istringstream parts(line);
                    std::string first_text;
                    std::string second_text;

                    parts >> first_text >> second_text;

                    // A one-celled ship needs one cell, and typing it twice is
                    // just as acceptable.
                    if (second_text.empty() && ship_class.size == 1)
                        second_text = first_text;

                    const std::optional<Coords> first = parse_cell(first_text);
                    const std::optional<Coords> second = parse_cell(second_text);

                    if (!first || !second)
                    {
                        out_ << "  Give two cells, a letter A-J and a row 1-10 each.\n";
                        continue;
                    }

                    const Placement ship = ordered(*first, *second);
                    const char* why = "";

                    if (!is_legal(placed, ship, ship_class.size, why))
                    {
                        out_ << "  No: " << why << ".\n";
                        continue;
                    }

                    placed.push_back(ship);
                    break;
                }
            }
        }

        draw(placed);

        return placed;
    }

} // namespace seabattle::console
