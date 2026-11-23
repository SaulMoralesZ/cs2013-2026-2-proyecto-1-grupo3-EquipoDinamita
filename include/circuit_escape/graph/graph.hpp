#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "../cells.hpp"
#include "../grid_runtime.hpp"
#include "../position.hpp"

namespace circuit_escape {

// Grafo del mapa: casillas = vertices, movimientos permitidos = aristas.
// Guarda los vecinos de cada casilla.
// Se construye a partir de un Grid (runtime, de Treicy).
class Graph {
public:
    // Construye el grafo a partir del grid: vertices = casillas libres transitables,
    // aristas = movimientos permitidos (vecinos transitables).
    explicit Graph(const RuntimeGrid& grid);

    // Lista de todas las casillas (vertices) del grafo.
    [[nodiscard]] std::vector<Position> vertices() const;

    // Vecinos alcanzables desde p (casillas transitables adyacentes).
    [[nodiscard]] std::vector<Position> neighbors(Position p) const;

    // Numero de vertices.
    [[nodiscard]] std::size_t vertexCount() const noexcept { return vertices_.size(); }

    // Indice de una posicion en la lista de vertices (-1 si no existe).
    [[nodiscard]] int indexOf(Position p) const;

private:
    std::vector<Position> vertices_;
    // adjacency list: neighbors[i] = lista de indices de vecinos de vertices_[i]
    std::vector<std::vector<int>> adjacency_;
};

}  // namespace circuit_escape