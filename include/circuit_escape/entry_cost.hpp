#pragma once

#include <variant>

#include "./cells.hpp"
#include "./game_rules.hpp"

namespace circuit_escape {

// Costo de energia de entrar a una celda. Lo usan el motor, el grafo
// (Dijkstra) y la ayuda H, para que preparacion, ayuda y juego coincidan.
// Solo cuentan la casilla base y la aspera; las recargas no son pesos negativos.
[[nodiscard]] inline int entry_cost(const Cell& cell, const GameRules& rules) {
    return std::visit(
        Overloaded{
            [&](const RoughTerrain&) { return rules.roughCost; },
            [&](const auto&) { return rules.baseCost; }},
        cell);
}

}  // namespace circuit_escape
