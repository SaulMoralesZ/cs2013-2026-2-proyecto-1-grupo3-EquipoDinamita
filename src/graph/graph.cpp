#include "../../include/circuit_escape/graph/graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace circuit_escape {

Graph::Graph(const RuntimeGrid& grid) {
    // Asignar indice a cada casilla transitable.
    std::unordered_map<Position, int, PositionHash> index;
    for (std::size_t r = 0; r < grid.rows(); ++r) {
        for (std::size_t c = 0; c < grid.columns(); ++c) {
            Position pos{r, c};
            if (isTraversable(grid.at(pos))) {
                index[pos] = static_cast<int>(vertices_.size());
                vertices_.push_back(pos);
            }
        }
    }

    // Construir lista de adyacencia.
    adjacency_.resize(vertices_.size());
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        Position p = vertices_[i];
        // Vecinos en orden fijo: arriba, abajo, izquierda, derecha
        // (coincide con el desempate del motor, seccion 3.2)
        for (Action action : {Action::up, Action::down, Action::left, Action::right}) {
            auto cand = grid.neighbor(p, action);
            if (cand && isTraversable(grid.at(*cand))) {
                auto it = index.find(*cand);
                if (it != index.end()) {
                    adjacency_[i].push_back(it->second);
                }
            }
        }
    }
}

std::vector<Position> Graph::vertices() const {
    return vertices_;
}

std::vector<Position> Graph::neighbors(Position p) const {
    int idx = indexOf(p);
    if (idx < 0) return {};
    std::vector<Position> result;
    for (int ni : adjacency_[static_cast<std::size_t>(idx)]) {
        result.push_back(vertices_[static_cast<std::size_t>(ni)]);
    }
    return result;
}

int Graph::indexOf(Position p) const {
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        if (vertices_[i] == p) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace circuit_escape