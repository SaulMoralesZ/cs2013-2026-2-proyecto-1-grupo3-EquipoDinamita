#pragma once

// ============================================================================
// COMPATIBILITY SHIM — Runtime Grid
// ============================================================================
// Este archivo es un SHIM de compatibilidad para el Bloque 2 (Miguel).
// Implementa la API de Grid con dimensiones en tiempo de ejecucion que Treicy
// (Bloque 1) prometio entregar. Cuando Treicy entregue su Grid runtime real,
// este archivo se elimina y se reemplaza por el de ella.
//
// API prometida por Treicy:
//   Grid<Cell>(filas, columnas)   constructor con dimensiones runtime
//   std::size_t rows() const      (runtime, no static)
//   std::size_t columns() const   (runtime, no static)
//   Cell& at(Position)            acceso por coordenada
//   bool contains(Position) const
//   begin()/end()/cbegin()/cend() iteradores row-major
//   static constexpr max_side = 100
//   std::optional<Position> neighbor(Position, Action) const
// ============================================================================

#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

#include "./cells.hpp"
#include "./position.hpp"

namespace circuit_escape {

class RuntimeGrid {
public:
    using value_type = Cell;
    using storage_type = std::vector<Cell>;
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    static constexpr std::size_t max_side = 100;

    // Constructor con dimensiones en tiempo de ejecucion.
    RuntimeGrid(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols), cells_(rows * cols) {
        if (rows == 0 || cols == 0) {
            throw std::invalid_argument("RuntimeGrid: dimensiones nulas");
        }
        if (rows > max_side || cols > max_side) {
            throw std::invalid_argument("RuntimeGrid: dimensiones exceden max_side");
        }
    }

    // Dimensiones runtime (no static).
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t columns() const noexcept { return cols_; }

    [[nodiscard]] bool contains(Position position) const noexcept {
        return position.row < rows_ && position.column < cols_;
    }

    Cell& at(Position position) {
        check(position);
        return cells_[position.row * cols_ + position.column];
    }

    const Cell& at(Position position) const {
        check(position);
        return cells_[position.row * cols_ + position.column];
    }

    Cell& operator()(std::size_t row, std::size_t column) {
        return at({row, column});
    }
    const Cell& operator()(std::size_t row, std::size_t column) const {
        return at({row, column});
    }

    [[nodiscard]] std::optional<Position> neighbor(Position position, Action action) const {
        const auto candidate = circuit_escape::neighbor(position, action);
        if (!candidate) return std::nullopt;
        if (!contains(*candidate)) return std::nullopt;
        return candidate;
    }

    iterator begin() noexcept { return cells_.begin(); }
    iterator end() noexcept { return cells_.end(); }
    const_iterator begin() const noexcept { return cells_.begin(); }
    const_iterator end() const noexcept { return cells_.end(); }
    const_iterator cbegin() const noexcept { return cells_.cbegin(); }
    const_iterator cend() const noexcept { return cells_.cend(); }

    // Convierte un indice lineal (orden por filas) en posicion.
    [[nodiscard]] Position positionOf(std::size_t index) const {
        return {index / cols_, index % cols_};
    }

private:
    void check(Position position) const {
        if (!contains(position)) {
            throw std::out_of_range("RuntimeGrid: posicion fuera del tablero");
        }
    }

    std::size_t rows_;
    std::size_t cols_;
    storage_type cells_;
};

// Alias para compatibilidad: cuando Treicy entregue su Grid runtime,
// cambiar este alias a su tipo.
using Grid = RuntimeGrid;

}  // namespace circuit_escape