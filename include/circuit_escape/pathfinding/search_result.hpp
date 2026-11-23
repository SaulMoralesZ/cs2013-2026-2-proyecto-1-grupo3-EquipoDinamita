#pragma once

#include <cstddef>
#include <vector>

#include "../position.hpp"

namespace circuit_escape {

// Resultado de una busqueda de camino (BFS o Dijkstra).
//  - path: casillas desde el origen hasta el destino, ambos incluidos.
//  - cost: Dijkstra = energia total (suma de entry_cost de cada casilla
//          entrada; el origen no cuenta). BFS = numero de movimientos.
//  - explored: casillas examinadas (extraidas de la cola) durante la busqueda.
// Si found es false, path esta vacio y cost no tiene significado.
struct SearchResult {
    bool found{false};
    int cost{0};
    std::vector<Position> path{};
    std::size_t explored{0};
};

}  // namespace circuit_escape
