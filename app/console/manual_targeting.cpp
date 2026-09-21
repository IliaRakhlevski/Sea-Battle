#include "manual_targeting.hpp"

#include "coord_format.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace seabattle::console {

    Coords ManualTargeting::next_target(const EnemyMap& map)
    {
        for (;;)
        {
            out_ << "Your shot (e.g. B5): ";

            std::string line;

            if (!std::getline(in_, line))
                throw std::runtime_error("input ended while waiting for a shot");

            const std::optional<Coords> cell = parse_cell(line);

            if (!cell)
            {
                out_ << "  Not a cell on this board. Use a letter A-J and a row 1-10.\n";
                continue;
            }

            if (map.cells()(cell->row, cell->col) != EnemyCellState::Unknown)
            {
                out_ << "  You already know what is there. Pick a cell marked '.'.\n";
                continue;
            }

            return *cell;
        }
    }

} // namespace seabattle::console
