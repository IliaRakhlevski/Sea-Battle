#pragma once
/**
 * @file grid2d.hpp
 * @brief Grid2D<T> - a fixed-size two-dimensional grid.
 *
 * Written for task 1 of the modern C++ practice track and reused here: the
 * own board and the knowledge map are both grids, and they differ only in
 * what a cell contains.
 *
 * The elements are kept in a single flat std::vector rather than a vector of
 * vectors. One allocation instead of one per row, every cell contiguous in
 * memory, and no way for the rows to end up with different lengths.
 *
 * The class follows the Rule of Zero: it owns nothing but a vector, so the
 * compiler-generated copy, move and destruction all do the right thing.
 *
 * @todo Leftovers from task 1, still to finish:
 *       - <iostream> is not needed here, only the tests use it;
 *       - <assert.h> should be <cassert>, the C++ spelling;
 *       - redundant const in "const auto begin() const";
 *       - [[nodiscard]] on size/rows/cols/at/begin/end;
 *       - cbegin() / cend();
 *       - MaxSizeT belongs in the class's private section: it is used only
 *         by checked_size, and as a namespace-scope constexpr it gets its own
 *         copy in every translation unit.
 */
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <limits>
#include <assert.h>

namespace seabattle {


/** @brief Largest representable std::size_t; used by the overflow guard. */
constexpr auto MaxSizeT = std::numeric_limits<std::size_t>::max();


/**
 * @brief A rectangular grid of T, sized once at construction.
 * @tparam T the element type; must be copy-constructible.
 */
template <typename T>
class Grid2D
{
    size_t rows_;           ///< Number of rows.
    size_t cols_;           ///< Number of columns.

    std::vector<T> data_;   ///< Cells, row-major, rows_ * cols_ of them.

    /**
     * @brief Unchecked access to a cell, const overload.
     * @param row row index; the caller guarantees it is in range.
     * @param col column index; the caller guarantees it is in range.
     * @return reference to the cell.
     *
     * Row-major: the cell (row, col) lives at row * cols_ + col.
     */
    const T& ref(size_t row, size_t col) const noexcept
    {
        return data_[row * cols_ + col];
    }

    /**
     * @brief Unchecked access to a cell.
     * @param row row index; the caller guarantees it is in range.
     * @param col column index; the caller guarantees it is in range.
     * @return reference to the cell.
     */
    T& ref(size_t row, size_t col) noexcept
    {
        return data_[row * cols_ + col];
    }

    /**
     * @brief Number of elements for a grid of the given shape.
     * @param rows number of rows; must not be zero.
     * @param cols number of columns; must not be zero.
     * @return rows * cols.
     * @throws std::length_error if either dimension is zero, or if the
     *         product would overflow std::size_t.
     *
     * The multiplication has to be guarded before it happens: once
     * rows * cols has wrapped around there is nothing left to inspect.
     * Division cannot overflow, so the test is written as a division.
     */
    static std::size_t checked_size(std::size_t rows, std::size_t cols)
    {
        if (cols == 0 || rows == 0 || (rows > MaxSizeT / cols))
            throw std::length_error("Invalid length");

        return rows * cols;
    }

public:

    /**
     * @brief Builds a grid of the given shape, every cell set to @p init.
     * @param rows number of rows; must not be zero.
     * @param cols number of columns; must not be zero.
     * @param init value copied into every cell.
     * @throws std::length_error if the shape is empty or too large.
     */
    Grid2D(size_t rows, size_t cols, const T& init = T{}) : rows_{ rows },
                                                            cols_{ cols },
                                                            data_(checked_size(rows, cols), init) {}

    /**
     * @brief Total number of cells.
     * @return rows() * cols().
     */
    size_t size() const noexcept
    {
        return data_.size();
    }

    /**
     * @brief Number of columns.
     * @return the width the grid was built with.
     */
    size_t cols() const noexcept
    {
        return cols_;
    }

    /**
     * @brief Number of rows.
     * @return the height the grid was built with.
     */
    size_t rows() const noexcept
    {
        return rows_;
    }

    /**
     * @brief Checked access to a cell.
     * @param row row index.
     * @param col column index.
     * @return reference to the cell.
     * @throws std::out_of_range if either index is outside the grid.
     *
     * Use this where the indices come from outside the program - user input,
     * a file, the network. Use operator() on a hot path where the caller has
     * already established that the indices are valid.
     */
    T& at(size_t row, size_t col)
    {
        if (row >= rows_ || col >= cols_)
            throw std::out_of_range("Grid2D::at: index out of range");
        return ref(row, col);
    }

    /**
     * @brief Checked access to a cell, const overload.
     * @param row row index.
     * @param col column index.
     * @return reference to the cell.
     * @throws std::out_of_range if either index is outside the grid.
     */
    const T& at(size_t row, size_t col) const {
        if (row >= rows_ || col >= cols_)
            throw std::out_of_range("Grid2D::at: index out of range");
        return ref(row, col);
    }

    /**
     * @brief Unchecked access to a cell.
     * @param row row index.
     * @param col column index.
     * @return reference to the cell.
     * @pre both indices are inside the grid; checked by assert in a debug build.
     */
    T& operator()(size_t row, size_t col)
    {
        assert(row < rows_ && col < cols_);
        return ref(row, col);
    }

    /**
     * @brief Unchecked access to a cell, const overload.
     * @param row row index.
     * @param col column index.
     * @return reference to the cell.
     * @pre both indices are inside the grid; checked by assert in a debug build.
     */
    const T& operator()(size_t row, size_t col) const
    {
        assert(row < rows_ && col < cols_);
        return ref(row, col);
    }

    /**
     * @brief Overwrites every cell with @p value.
     * @param value the value to copy into each cell.
     */
    void fill(const T& value)
    {
        std::fill(data_.begin(), data_.end(), value);
    }

    /** @brief Iterator to the first cell, in row-major order. */
    auto begin()
    {
        return data_.begin();
    }

    /** @brief Iterator to the first cell, const overload. */
    const auto begin() const
    {
        return data_.begin();
    }

    /** @brief Iterator one past the last cell. */
    auto end()
    {
        return data_.end();
    }

    /** @brief Iterator one past the last cell, const overload. */
    const auto end() const
    {
        return data_.end();
    }
};

} // namespace seabattle
