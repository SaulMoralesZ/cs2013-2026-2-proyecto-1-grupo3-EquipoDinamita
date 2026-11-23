#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "../graph/graph.hpp"
#include "../position.hpp"
#include "./search_result.hpp"

namespace circuit_escape {

// Arbol de caminos de menor gasto desde un origen (una ejecucion de Dijkstra).
// Una sola ejecucion da el costo hacia TODOS los destinos: la validacion
// (inicio, A, B) y la ayuda H (posicion actual, A, B) la reutilizan.
//
// Guarda una referencia al grafo: el Graph debe vivir mas que este objeto.
class ShortestPaths {
public:
    ShortestPaths(const Graph& graph, Position source, std::vector<int> distance,
                  std::vector<int> previous, std::size_t explored);

    [[nodiscard]] Position source() const noexcept { return source_; }
    [[nodiscard]] std::size_t explored() const noexcept { return explored_; }

    // Menor gasto de energia hasta target; nullopt si es inaccesible.
    [[nodiscard]] std::optional<int> cost_to(Position target) const;

    // Camino de menor gasto reconstruido con las casillas anteriores.
    [[nodiscard]] SearchResult path_to(Position target) const;

private:
    const Graph* graph_;
    Position source_;
    std::vector<int> distance_;  // -1 = inaccesible
    std::vector<int> previous_;  // -1 = sin anterior (origen o inaccesible)
    std::size_t explored_;
};

// Dijkstra propio con std::priority_queue. Peso de cada movimiento = energia
// de entrar en la casilla destino (no negativo). Empates: a igual distancia
// se extrae primero el vertice de menor indice (orden por filas) y solo se
// reemplaza un anterior con una distancia estrictamente menor -> resultado
// reproducible. Lanza std::invalid_argument si source no es transitable.
[[nodiscard]] ShortestPaths dijkstra(const Graph& graph, Position source);

// Atajo: camino de menor gasto entre dos casillas.
[[nodiscard]] SearchResult dijkstra_path(const Graph& graph, Position from, Position to);

}  // namespace circuit_escape
