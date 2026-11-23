#pragma once

#include <cstddef>
#include <vector>

#include "../cells.hpp"
#include "../game_rules.hpp"
#include "../grid.hpp"
#include "../position.hpp"

namespace circuit_escape {

// Grafo del mapa: casillas transitables = vertices, movimientos permitidos =
// aristas. Cada vertice guarda el costo de energia de ENTRAR en el
// (entry_cost), que es el peso de toda arista que llega a el.
// Los vecinos se guardan en orden fijo: arriba, abajo, izquierda, derecha
// (desempate reproducible para BFS y Dijkstra).
// Los muros son fijos durante la partida: el grafo se construye una vez por mapa.
class Graph {
public:
    explicit Graph(const Grid<Cell>& grid, const GameRules& rules = GameRules{});

    // Lista de todas las casillas (vertices) del grafo, en orden por filas.
    [[nodiscard]] std::vector<Position> vertices() const;

    // Vecinos alcanzables desde p (casillas transitables adyacentes).
    [[nodiscard]] std::vector<Position> neighbors(Position p) const;

    [[nodiscard]] std::size_t vertexCount() const noexcept { return vertices_.size(); }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }

    // Indice de una posicion en la lista de vertices (-1 si no es vertice). O(1).
    [[nodiscard]] int indexOf(Position p) const;

    // --- Acceso por indice (lo usan BFS y Dijkstra) ---
    [[nodiscard]] Position position(std::size_t vertex) const { return vertices_.at(vertex); }
    [[nodiscard]] const std::vector<int>& neighbor_indices(std::size_t vertex) const {
        return adjacency_.at(vertex);
    }
    // Energia que cuesta entrar en el vertice.
    [[nodiscard]] int entry_cost(std::size_t vertex) const { return costs_.at(vertex); }

private:
    std::size_t rows_;
    std::size_t columns_;
    std::vector<Position> vertices_;
    std::vector<int> costs_;
    // adjacency_[i] = indices de los vecinos de vertices_[i]
    std::vector<std::vector<int>> adjacency_;
    // cell_index_[fila * columnas + columna] = indice de vertice o -1
    std::vector<int> cell_index_;
};

}  // namespace circuit_escape
