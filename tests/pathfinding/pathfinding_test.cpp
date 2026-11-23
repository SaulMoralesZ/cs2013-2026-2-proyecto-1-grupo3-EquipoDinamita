#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "circuit_escape/cells.hpp"
#include "circuit_escape/game_rules.hpp"
#include "circuit_escape/graph/graph.hpp"
#include "circuit_escape/grid.hpp"
#include "circuit_escape/map_generation/map_builder.hpp"
#include "circuit_escape/pathfinding/bfs.hpp"
#include "circuit_escape/pathfinding/dijkstra.hpp"

using namespace circuit_escape;

namespace {

// Tablero con digitos de costo: '#' muro, '3' terreno aspero (roughCost = 3),
// cualquier otro simbolo = casilla normal de costo 1.
Grid<Cell> boardFrom(const std::vector<std::string>& rows) {
    Grid<Cell> grid(rows.size(), rows.front().size());
    for (std::size_t r = 0; r < rows.size(); ++r)
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const char ch = rows[r][c];
            if (ch == '#') grid.at({r, c}) = Wall{};
            else if (ch == '3') grid.at({r, c}) = RoughTerrain{};
            else if (ch == 'S') grid.at({r, c}) = Exit{};
            else grid.at({r, c}) = Empty{};
        }
    return grid;
}

GameRules costRules(int base, int rough) {
    GameRules rules;
    rules.baseCost = base;
    rules.roughCost = rough;
    return rules;
}

bool adjacent(Position a, Position b) { return manhattanDistance(a, b) == 1; }

// Camino valido: empieza y termina donde debe, pasos adyacentes y sin muros.
void assertValidPath(const Grid<Cell>& grid, const SearchResult& r, Position from, Position to) {
    assert(r.found);
    assert(r.path.front() == from);
    assert(r.path.back() == to);
    for (std::size_t i = 0; i < r.path.size(); ++i) {
        assert(isTraversable(grid.at(r.path[i])));
        if (i > 0) assert(adjacent(r.path[i - 1], r.path[i]));
    }
}

// Ejemplo de la seccion 3.3 del enunciado.
const std::vector<std::string> kExample = {
    "#########",
    "#I333A1S#",
    "#1###1#1#",
    "#B111V11#",
    "#########",
};
constexpr Position kI{1, 1}, kA{1, 5}, kS{1, 7}, kB{3, 1};

void dijkstraMatchesStatementExample() {
    const auto grid = boardFrom(kExample);
    const Graph graph(grid, costRules(1, 3));

    const ShortestPaths fromI = dijkstra(graph, kI);
    assert(fromI.cost_to(kA) == 8);  // por abajo, no por los 3
    assert(fromI.cost_to(kB) == 2);

    const ShortestPaths fromA = dijkstra(graph, kA);
    const ShortestPaths fromB = dijkstra(graph, kB);
    assert(fromB.cost_to(kA) == 6);
    assert(fromA.cost_to(kS) == 2);
    assert(fromB.cost_to(kS) == 8);

    const int viaAB = *fromI.cost_to(kA) + *fromA.cost_to(kB) + *fromB.cost_to(kS);
    const int viaBA = *fromI.cost_to(kB) + *fromB.cost_to(kA) + *fromA.cost_to(kS);
    assert(viaAB == 22);
    assert(viaBA == 10);  // "Gasto total: 10"

    const SearchResult toB = fromI.path_to(kB);
    assertValidPath(grid, toB, kI, kB);
    assert((toB.path == std::vector<Position>{kI, {2, 1}, kB}));
    assert(toB.cost == 2);
    assert(toB.explored > 0 && toB.explored <= graph.vertexCount());

    // El camino a A esquiva el terreno de costo 3.
    const SearchResult toA = fromI.path_to(kA);
    assertValidPath(grid, toA, kI, kA);
    for (Position p : toA.path) assert(!std::holds_alternative<RoughTerrain>(grid.at(p)));
}

void dijkstraVersusBfsDiffer() {
    // BFS minimiza movimientos (4, cruzando los 3); Dijkstra minimiza energia.
    const auto grid = boardFrom(kExample);
    const Graph graph(grid, costRules(1, 3));
    const SearchResult bfs = bfs_path(graph, kI, kA);
    assertValidPath(grid, bfs, kI, kA);
    assert(bfs.cost == 4);
    assert(dijkstra_path(graph, kI, kA).path.size() - 1 > 4);
}

void inaccessibleTarget() {
    const auto grid = boardFrom({
        "..#..",
        "..#..",
    });
    const Graph graph(grid);
    const ShortestPaths paths = dijkstra(graph, {0, 0});
    assert(!paths.cost_to({0, 4}).has_value());
    assert(!paths.path_to({0, 4}).found);
    assert(paths.path_to({0, 4}).path.empty());
    assert(!bfs_path(graph, {0, 0}, {0, 4}).found);
    assert(!bfs_next_action(graph, {0, 0}, {0, 4}).has_value());
    // Un muro como destino tampoco es alcanzable.
    assert(!paths.cost_to({0, 2}).has_value());
    assert(!bfs_path(graph, {0, 0}, {0, 2}).found);
}

void sourceMustBeTraversable() {
    const auto grid = boardFrom({".#."});
    const Graph graph(grid);
    bool threw = false;
    try {
        (void)dijkstra(graph, {0, 1});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    threw = false;
    try {
        (void)bfs_path(graph, {5, 5}, {0, 0});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void sameCellIsZero() {
    const auto grid = boardFrom({"..."});
    const Graph graph(grid);
    assert(dijkstra(graph, {0, 1}).cost_to({0, 1}) == 0);
    const SearchResult r = bfs_path(graph, {0, 1}, {0, 1});
    assert(r.found && r.cost == 0 && r.path.size() == 1);
    assert(!bfs_next_action(graph, {0, 1}, {0, 1}).has_value());  // ya llego: espera
}

void fixedTieBreak() {
    // Tablero abierto: (0,0) -> (1,1) tiene dos caminos de 2 pasos.
    // Orden de vecinos arriba, abajo, izquierda, derecha => se baja primero.
    const auto grid = boardFrom({"...", "...", "..."});
    const Graph graph(grid);
    const SearchResult bfs = bfs_path(graph, {0, 0}, {1, 1});
    assert((bfs.path == std::vector<Position>{{0, 0}, {1, 0}, {1, 1}}));
    assert(bfs_next_action(graph, {0, 0}, {1, 1}) == Action::down);
    for (int i = 0; i < 5; ++i) {  // siempre el mismo resultado
        assert(bfs_path(graph, {0, 0}, {1, 1}).path == bfs.path);
        assert(dijkstra_path(graph, {0, 0}, {2, 2}).path ==
               dijkstra_path(graph, {0, 0}, {2, 2}).path);
    }
    assert(bfs_next_action(graph, {2, 2}, {0, 0}) == Action::up);
}

// Seccion 7: con cada paso de costo uno, Dijkstra coincide en costo con BFS.
void dijkstraEqualsBfsWithUnitCosts() {
    MapConfig config;
    config.numResources = 2;
    config.numBatteries = 2;
    config.numTraps = 2;
    for (unsigned seed = 1; seed <= 5; ++seed) {
        const Map map = MapBuilder::buildMap(1, seed, 20, 30, config);
        const Graph graph(map.grid, costRules(1, 1));
        const ShortestPaths fromStart = dijkstra(graph, map.start);
        for (Position target : graph.vertices()) {
            const SearchResult bfs = bfs_path(graph, map.start, target);
            // Kruskal conecta todas las casillas: todo es alcanzable.
            assert(bfs.found);
            assert(fromStart.cost_to(target) == bfs.cost);
        }
    }
}

void graphCostsAndIndex() {
    const auto grid = boardFrom({".3#", "S.."});
    const Graph graph(grid, costRules(1, 3));
    assert(graph.vertexCount() == 5);
    assert(graph.indexOf({0, 2}) == -1);      // muro
    assert(graph.indexOf({9, 9}) == -1);      // fuera
    assert(graph.entry_cost(static_cast<std::size_t>(graph.indexOf({0, 1}))) == 3);
    assert(graph.entry_cost(static_cast<std::size_t>(graph.indexOf({1, 0}))) == 1);
    for (Position p : graph.vertices()) {
        assert(graph.position(static_cast<std::size_t>(graph.indexOf(p))) == p);
    }
}

}  // namespace

int main() {
    graphCostsAndIndex();
    dijkstraMatchesStatementExample();
    dijkstraVersusBfsDiffer();
    inaccessibleTarget();
    sourceMustBeTraversable();
    sameCellIsZero();
    fixedTieBreak();
    dijkstraEqualsBfsWithUnitCosts();
    return 0;
}
