#include "../../include/circuit_escape/pathfinding/dijkstra.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace circuit_escape {

ShortestPaths::ShortestPaths(const Graph& graph, Position source, std::vector<int> distance,
                             std::vector<int> previous, std::size_t explored)
    : graph_(&graph),
      source_(source),
      distance_(std::move(distance)),
      previous_(std::move(previous)),
      explored_(explored) {}

std::optional<int> ShortestPaths::cost_to(Position target) const {
    const int t = graph_->indexOf(target);
    if (t < 0 || distance_[static_cast<std::size_t>(t)] < 0) return std::nullopt;
    return distance_[static_cast<std::size_t>(t)];
}

SearchResult ShortestPaths::path_to(Position target) const {
    SearchResult result;
    result.explored = explored_;
    const auto cost = cost_to(target);
    if (!cost) return result;

    result.found = true;
    result.cost = *cost;
    // Reconstruccion hacia atras con las casillas anteriores.
    for (int v = graph_->indexOf(target); v >= 0; v = previous_[static_cast<std::size_t>(v)]) {
        result.path.push_back(graph_->position(static_cast<std::size_t>(v)));
    }
    std::reverse(result.path.begin(), result.path.end());
    return result;
}

ShortestPaths dijkstra(const Graph& graph, Position source) {
    const int s = graph.indexOf(source);
    if (s < 0) {
        throw std::invalid_argument("dijkstra: el origen " + toString(source) +
                                    " no es una casilla transitable");
    }

    const std::size_t n = graph.vertexCount();
    std::vector<int> distance(n, -1);
    std::vector<int> previous(n, -1);
    std::vector<bool> settled(n, false);
    std::size_t explored = 0;

    // Min-heap de (distancia, vertice): a igual distancia sale el menor indice.
    using Entry = std::pair<int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
    distance[static_cast<std::size_t>(s)] = 0;
    frontier.emplace(0, s);

    while (!frontier.empty()) {
        const auto [d, u] = frontier.top();
        frontier.pop();
        const auto ui = static_cast<std::size_t>(u);
        if (settled[ui]) continue;  // entrada obsoleta
        settled[ui] = true;
        ++explored;

        for (int v : graph.neighbor_indices(ui)) {
            const auto vi = static_cast<std::size_t>(v);
            if (settled[vi]) continue;
            const int candidate = d + graph.entry_cost(vi);
            if (distance[vi] < 0 || candidate < distance[vi]) {
                distance[vi] = candidate;
                previous[vi] = u;
                frontier.emplace(candidate, v);
            }
        }
    }

    return ShortestPaths(graph, source, std::move(distance), std::move(previous), explored);
}

SearchResult dijkstra_path(const Graph& graph, Position from, Position to) {
    return dijkstra(graph, from).path_to(to);
}

}  // namespace circuit_escape
