#pragma once

#include <optional>

#include "../graph/graph.hpp"
#include "../position.hpp"
#include "./search_result.hpp"

namespace circuit_escape {

// BFS propio: camino con MENOS MOVIMIENTOS (ignora los costos de energia).
// Lo usa el vigilante. Vecinos en orden fijo (arriba, abajo, izquierda,
// derecha) y la primera visita de cada casilla es la definitiva, asi que el
// camino elegido entre varios igual de cortos siempre es el mismo.
// cost = numero de movimientos. Si from == to, el camino es {from} y cost 0.
// Lanza std::invalid_argument si from no es transitable.
[[nodiscard]] SearchResult bfs_path(const Graph& graph, Position from, Position to);

// Primer movimiento del camino BFS de from hacia to.
// nullopt si no hay camino o si ya esta en to (el vigilante espera).
[[nodiscard]] std::optional<Action> bfs_next_action(const Graph& graph, Position from,
                                                    Position to);

}  // namespace circuit_escape
