#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "./position.hpp"

namespace circuit_escape {


// ------------------------------------------------------------------ Grid
template<typename CellType>
class Grid {
public:
    using value_type = CellType;
    using storage_type = std::vector<CellType>;
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    static constexpr std::size_t max_side = 100;

    Grid(std::size_t rows, std::size_t columns) : rows_(rows), columns_(columns), cells_(checked_size(rows,columns)) {}

    std::size_t rows() const noexcept { return rows_; };
    std::size_t columns() const noexcept { return columns_; };

    [[nodiscard]] bool contains(Position position) const noexcept {
        return position.row < rows_ && position.column < columns_;
    }

    CellType& at(Position position) {
        check(position);
        return cells_[position.row * columns_ + position.column];
    }

    const CellType& at(Position position) const {
        check(position);
        return cells_[position.row * columns_ + position.column];
    }

    CellType& operator()(std::size_t row, std::size_t column) {
        return at({row, column});
    }

    const CellType& operator()(std::size_t row, std::size_t column) const {
        return at({row, column});
    }

    // Convierte un índice lineal (orden por filas) en posición.
    [[nodiscard]] Position positionOf(std::size_t index) const noexcept {
        return {index / columns_, index % columns_};
    }

    [[nodiscard]] std::optional<Position> neighbor(Position origin, Action action) const {
        return std::nullopt;
    }
    iterator begin() noexcept { return cells_.begin(); }
    iterator end() noexcept { return cells_.end(); }
    const_iterator begin() const noexcept { return cells_.begin(); }
    const_iterator end() const noexcept { return cells_.end(); }
    const_iterator cbegin() const noexcept { return cells_.cbegin(); }
    const_iterator cend() const noexcept { return cells_.cend(); }

private:
    std::size_t rows_;
    std::size_t columns_;
    storage_type cells_;
    static std::size_t checked_size(std::size_t rows, std::size_t columns) {
        if (rows == 0 || columns == 0) {
            throw std::invalid_argument("Grid: filas y columnas deben ser mayores que 0");
        }
        if (rows>max_side || columns>max_side) {
            throw std::invalid_argument("Grid: el maximo es " + std::to_string(max_side) + " filas y " + std::to_string(max_side) + " columnas ");
        }
        return rows * columns;
    }
    void check(Position position) const {
        if (!contains(position)) {
            throw std::out_of_range("Grid: posicion fuera del tablero " + toString(position));
        }
    }
};



// ------------------------------------------- Templates de función (rangos)

// Cuenta los elementos de [first, last) que cumplen pred.
template<typename InputIt, typename Predicate>
[[nodiscard]] constexpr std::size_t countCells(InputIt first, InputIt last, Predicate pred) {
    std::size_t count = 0;
    for (; first != last; ++first) {
        if (pred(*first)) ++count;
    }
    return count;
}

// Primer elemento que cumple pred; devuelve last si no existe.
template<typename InputIt, typename Predicate>
[[nodiscard]] constexpr InputIt findFirstCell(InputIt first, InputIt last, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) return first;
    }
    return last;
}

// Posición de la primera celda que cumple pred, si existe.
template<typename CellType, typename Predicate>
[[nodiscard]] std::optional<Position> findPosition(
    const Grid<CellType>& grid, Predicate pred) {
    const auto it = findFirstCell(grid.begin(), grid.end(), pred);
    if (it == grid.end()) return std::nullopt;
    return grid.positionOf(
        static_cast<std::size_t>(std::distance(grid.begin(), it)));
}

}  // namespace circuit_escape