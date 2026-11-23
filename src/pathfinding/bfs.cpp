#include "../../include/circuit_escape/pathfinding/bfs.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <vector>

namespace circuit_escape {

SearchResult bfs_path(const Graph& graph, Position from, Position to) {
    const int s = graph.indexOf(from);
    if (s < 0) {
        throw std::invalid_argument("bfs: el origen " + toString(from) +
                                    " no es una casilla transitable");
    }

    SearchResult result;
    const int t = graph.indexOf(to);
    if (t < 0) return result;  // destino no transitable: sin camino

    const std::size_t n = graph.vertexCount();
    std::vector<int> previous(n, -1);
    std::vector<bool> visited(n, false);
    std::queue<int> frontier;
    visited[static_cast<std::size_t>(s)] = true;
    frontier.push(s);

    while (!frontier.empty()) {
        const int u = frontier.front();
        frontier.pop();
        ++result.explored;
        if (u == t) {
            result.found = true;
            break;
        }
        for (int v : graph.neighbor_indices(static_cast<std::size_t>(u))) {
            const auto vi = static_cast<std::size_t>(v);
            if (visited[vi]) continue;
            visited[vi] = true;
            previous[vi] = u;
            frontier.push(v);
        }
    }

    if (!result.found) return result;
    for (int v = t; v >= 0; v = previous[static_cast<std::size_t>(v)]) {
        result.path.push_back(graph.position(static_cast<std::size_t>(v)));
    }
    std::reverse(result.path.begin(), result.path.end());
    result.cost = static_cast<int>(result.path.size()) - 1;
    return result;
}

std::optional<Action> bfs_next_action(const Graph& graph, Position from, Position to) {
    const SearchResult route = bfs_path(graph, from, to);
    if (!route.found || route.path.size() < 2) return std::nullopt;
    return actionBetween(route.path[0], route.path[1]);
}

}  // namespace circuit_escape
