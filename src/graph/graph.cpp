#include "../../include/circuit_escape/graph/graph.hpp"

#include "../../include/circuit_escape/entry_cost.hpp"

namespace circuit_escape {

Graph::Graph(const Grid<Cell>& grid, const GameRules& rules)
    : rows_(grid.rows()),
      columns_(grid.columns()),
      cell_index_(grid.rows() * grid.columns(), -1) {
    // Asignar indice a cada casilla transitable (orden por filas).
    for (std::size_t r = 0; r < rows_; ++r) {
        for (std::size_t c = 0; c < columns_; ++c) {
            const Position pos{r, c};
            const Cell& cell = grid.at(pos);
            if (isTraversable(cell)) {
                cell_index_[r * columns_ + c] = static_cast<int>(vertices_.size());
                vertices_.push_back(pos);
                costs_.push_back(circuit_escape::entry_cost(cell, rules));
            }
        }
    }

    // Lista de adyacencia con orden de vecinos fijo.
    adjacency_.resize(vertices_.size());
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        for (Action action : {Action::up, Action::down, Action::left, Action::right}) {
            const auto candidate = grid.neighbor(vertices_[i], action);
            if (!candidate) continue;
            const int j = indexOf(*candidate);
            if (j >= 0) adjacency_[i].push_back(j);
        }
    }
}

std::vector<Position> Graph::vertices() const {
    return vertices_;
}

std::vector<Position> Graph::neighbors(Position p) const {
    const int idx = indexOf(p);
    if (idx < 0) return {};
    std::vector<Position> result;
    for (int ni : adjacency_[static_cast<std::size_t>(idx)]) {
        result.push_back(vertices_[static_cast<std::size_t>(ni)]);
    }
    return result;
}

int Graph::indexOf(Position p) const {
    if (p.row >= rows_ || p.column >= columns_) return -1;
    return cell_index_[p.row * columns_ + p.column];
}

}  // namespace circuit_escape
