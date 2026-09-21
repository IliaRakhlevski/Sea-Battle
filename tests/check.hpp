#pragma once
/**
 * @file check.hpp
 * @brief The whole test "framework": one macro and one summary.
 *
 * Every test in this directory is an ordinary program. It runs its checks,
 * prints what failed, and returns 0 if nothing did and 1 otherwise. CTest -
 * and Visual Studio's test list - only look at that return value.
 *
 * CHECK prints the expression it was given, so a failure reads as the very
 * line of code that failed, with the file and line number in front of it.
 */

#include <iostream>

namespace seabattle::test {

    /** @brief How many checks have failed so far in this program. */
    inline int& failures()
    {
        static int count = 0;
        return count;
    }

    /**
     * @brief Records one check.
     *
     * Only the first few failures are printed: a check inside a loop over a
     * thousand games would otherwise bury the one line that matters.
     */
    inline void check(bool ok, const char* expression, const char* file, int line)
    {
        if (ok)
            return;

        if (++failures() <= 20)
            std::cout << file << ':' << line << ": FAIL: " << expression << '\n';
    }

    /** @brief Prints the verdict and gives main() its return value. */
    inline int report(const char* name)
    {
        if (failures() == 0)
        {
            std::cout << name << ": all checks passed\n";
            return 0;
        }

        std::cout << name << ": " << failures() << " check(s) failed\n";
        return 1;
    }

} // namespace seabattle::test

#define CHECK(condition) ::seabattle::test::check((condition), #condition, __FILE__, __LINE__)
