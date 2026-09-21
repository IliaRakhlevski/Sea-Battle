#include "console_view.hpp"

#include "coord_format.hpp"

#include <cstddef>
#include <ostream>
#include <variant>

namespace seabattle::console {

    namespace {

        /** @brief The gap between the two boards. */
        const char* const gap = "     ";

        // One alphabet for both boards. A symbol means the same thing on the
        // left as on the right, which is what makes a single legend honest:
        //
        //   .  water, or a cell nothing is known about
        //   o  a shot that found water
        //   #  an undamaged deck - only ever visible on one's own board
        //   X  a damaged deck of a ship that is still afloat
        //   *  a cell of a ship that has gone down
        //   ~  deduced to be empty - only ever appears on the knowledge map

        /**
         * @brief How one cell of the owner's board is drawn.
         * @param board the owner's board.
         * @param at    the cell to draw.
         *
         * The owner sees everything, their intact ships included - this is the
         * board nobody else may look at. Whether a deck belongs to a ship that
         * has gone down is a question for the board: the cell itself only knows
         * that it was hit, and a damaged ship and a dead one would otherwise
         * look identical to the person defending them.
         */
        char symbol_of(const OwnBoard& board, const Coords& at)
        {
            const Cell& cell = board.cells()(at.row, at.col);

            if (const WaterCell* water = std::get_if<WaterCell>(&cell))
                return water->state == WaterState::Untouched ? '.' : 'o';

            if (board.is_sunk_at(at))
                return '*';

            return std::get<ShipCell>(cell).state == DeckState::Intact ? '#' : 'X';
        }

        /**
         * @brief How one cell of the knowledge map is drawn.
         * @param state what is known about the cell.
         *
         * Missed and Crossed both mean "no ship here", and they are drawn
         * differently on purpose: one of them cost a shot, the other did not,
         * and the player wants to see where the shots actually went.
         *
         * There is no symbol for an undamaged deck, because that is exactly
         * what a player never learns about the opponent.
         */
        char symbol_of(EnemyCellState state)
        {
            switch (state)
            {
            case EnemyCellState::Unknown: return '.';
            case EnemyCellState::Missed:  return 'o';
            case EnemyCellState::Hit:     return 'X';
            case EnemyCellState::Sunk:    return '*';
            case EnemyCellState::Crossed: return '~';
            }

            return '?';   // unreachable for a valid enumerator
        }

        /** @brief The word for one outcome. */
        const char* text_of(ShotResult result)
        {
            switch (result)
            {
            case ShotResult::Miss: return "miss";
            case ShotResult::Hit:  return "hit";
            case ShotResult::Sunk: return "sunk";
            }

            return "?";
        }

    } // namespace

    void ConsoleView::draw_column_header()
    {
        out_ << "   ";

        for (std::size_t col = 0; col < board_size; ++col)
            out_ << ' ' << static_cast<char>('A' + static_cast<int>(col));
    }

    void ConsoleView::draw_boards()
    {
        const Player& me = session_.player(side_);

        out_ << "\n     YOUR FLEET" << gap << "            ENEMY WATERS\n";

        draw_column_header();
        out_ << gap;
        draw_column_header();
        out_ << '\n';

        for (std::size_t row = 0; row < board_size; ++row)
        {
            // Rows are numbered from one on screen and from zero in memory;
            // this line and coord_format.cpp are the only two places that know it.
            out_ << (row + 1 < 10 ? " " : "") << (row + 1) << " ";

            for (std::size_t col = 0; col < board_size; ++col)
                out_ << ' ' << symbol_of(me.own_board(), Coords{ row, col });

            out_ << gap;

            out_ << (row + 1 < 10 ? " " : "") << (row + 1) << " ";

            for (std::size_t col = 0; col < board_size; ++col)
                out_ << ' ' << symbol_of(me.enemy_map().cells()(row, col));

            out_ << '\n';
        }

        out_ << "\n  . water   o miss   # your ship   X hit   * sunk"
                "   ~ empty by deduction\n";
    }

    void ConsoleView::on_fleets_deployed()
    {
        out_ << "\nBoth fleets are in position.\n";
        draw_boards();
    }

    void ConsoleView::on_shot(Side shooter, const Coords& at, ShotResult result)
    {
        draw_boards();

        out_ << (shooter == side_ ? "You fired at " : "The enemy fired at ")
             << to_text(at) << " - " << text_of(result) << ".\n";
    }

    void ConsoleView::on_game_over(Side winner)
    {
        out_ << (winner == side_ ? "\nYou win.\n" : "\nYou lose.\n");
    }

} // namespace seabattle::console
