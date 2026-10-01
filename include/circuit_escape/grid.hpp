#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace circuit_escape {

// ------------------------------------------------------ Posición y acción
struct Position {
    std::size_t row{};
    std::size_t column{};
    friend bool operator==(const Position&, const Position&) = default;
};

struct PositionHash {
    std::size_t operator()(const Position& p) const noexcept {
        return std::hash<std::size_t>{}(p.row) * 1000003u ^
               std::hash<std::size_t>{}(p.column);
    }
};

enum class Action { up, down, left, right, wait };

// No conoce el tamaño del tablero: solo falla ante coordenadas negativas.
[[nodiscard]] inline std::optional<Position> neighbor(Position origin, Action action) {
    switch (action) {
        case Action::up:
            if (origin.row == 0) return std::nullopt;
            return Position{origin.row - 1, origin.column};
        case Action::down:
            return Position{origin.row + 1, origin.column};
        case Action::left:
            if (origin.column == 0) return std::nullopt;
            return Position{origin.row, origin.column - 1};
        case Action::right:
            return Position{origin.row, origin.column + 1};
        case Action::wait:
            return origin;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string toString(Position position) {
    return "(" + std::to_string(position.row) + "," +
           std::to_string(position.column) + ")";
}

[[nodiscard]] inline std::string_view toString(Action action) {
    switch (action) {
        case Action::up: return "arriba";
        case Action::down: return "abajo";
        case Action::left: return "izquierda";
        case Action::right: return "derecha";
        case Action::wait: return "esperar";
    }
    return "?";
}

[[nodiscard]] inline std::size_t manhattan(Position a, Position b) noexcept {
    const auto diff = [](std::size_t x, std::size_t y) { return x > y ? x - y : y - x; };
    return diff(a.row, b.row) + diff(a.column, b.column);
}

// ------------------------------------------------------------------ Grid
template<typename CellType, std::size_t Rows, std::size_t Columns>
class Grid {
    static_assert(Rows > 0 && Columns > 0,
                  "Grid requiere al menos una fila y una columna");

public:
    using value_type = CellType;
    using storage_type = std::array<CellType, Rows * Columns>;
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    static constexpr std::size_t rows() noexcept { return Rows; }
    static constexpr std::size_t columns() noexcept { return Columns; }

    [[nodiscard]] constexpr bool contains(Position position) const noexcept {
        return position.row < Rows && position.column < Columns;
    }

    CellType& at(Position position) {
        check(position);
        return cells_[position.row * Columns + position.column];
    }

    const CellType& at(Position position) const {
        check(position);
        return cells_[position.row * Columns + position.column];
    }

    CellType& operator()(std::size_t row, std::size_t column) {
        return at({row, column});
    }
    const CellType& operator()(std::size_t row, std::size_t column) const {
        return at({row, column});
    }

    // Convierte un índice lineal (orden por filas) en posición.
    [[nodiscard]] static constexpr Position positionOf(std::size_t index) noexcept {
        return {index / Columns, index % Columns};
    }

    iterator begin() noexcept { return cells_.begin(); }
    iterator end() noexcept { return cells_.end(); }
    const_iterator begin() const noexcept { return cells_.begin(); }
    const_iterator end() const noexcept { return cells_.end(); }
    const_iterator cbegin() const noexcept { return cells_.cbegin(); }
    const_iterator cend() const noexcept { return cells_.cend(); }

private:
    void check(Position position) const {
        if (!contains(position)) {
            throw std::out_of_range("Grid: posicion fuera del tablero " +
                                    toString(position));
        }
    }

    storage_type cells_{};
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
template<typename CellType, std::size_t Rows, std::size_t Columns, typename Predicate>
[[nodiscard]] std::optional<Position> findPosition(
    const Grid<CellType, Rows, Columns>& grid, Predicate pred) {
    const auto it = findFirstCell(grid.begin(), grid.end(), pred);
    if (it == grid.end()) return std::nullopt;
    return Grid<CellType, Rows, Columns>::positionOf(
        static_cast<std::size_t>(std::distance(grid.begin(), it)));
}

}  // namespace circuit_escape