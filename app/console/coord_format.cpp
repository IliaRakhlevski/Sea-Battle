#include "coord_format.hpp"

#include "seabattle/rules.hpp"

#include <cctype>
#include <cstddef>

namespace seabattle::console {

    namespace {

        /** @brief Whether the text holds only digits and at least one. */
        bool all_digits(const std::string& text)
        {
            if (text.empty())
                return false;

            for (const char c : text)
                if (std::isdigit(static_cast<unsigned char>(c)) == 0)
                    return false;

            return true;
        }

    } // namespace

    std::string to_text(const Coords& cell)
    {
        // Columns are letters starting at 'A', rows are numbers starting at 1:
        // the board a player sees is one-based, the arrays behind it are not.
        std::string text;
        text += static_cast<char>('A' + static_cast<int>(cell.col));
        text += std::to_string(cell.row + 1);

        return text;
    }

    std::optional<Coords> parse_cell(const std::string& text)
    {
        std::size_t first = text.find_first_not_of(" \t");
        const std::size_t last = text.find_last_not_of(" \t");

        if (first == std::string::npos)
            return std::nullopt;

        const std::string trimmed = text.substr(first, last - first + 1);

        if (trimmed.size() < 2)
            return std::nullopt;

        const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(trimmed[0])));

        if (letter < 'A' || letter > static_cast<char>('A' + static_cast<int>(board_size) - 1))
            return std::nullopt;

        const std::string digits = trimmed.substr(1);

        if (!all_digits(digits))
            return std::nullopt;

        // A row typed as "007" is still row 7; what matters is the value, and
        // anything above the board is rejected below.
        const unsigned long row_number = std::stoul(digits);

        if (row_number < 1 || row_number > board_size)
            return std::nullopt;

        return Coords{ static_cast<std::size_t>(row_number) - 1,
                       static_cast<std::size_t>(letter - 'A') };
    }

} // namespace seabattle::console
