#undef NDEBUG
#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "../include/circuit_escape/cells.hpp"
#include "../include/circuit_escape/graph/disjoint_set.hpp"
#include "../include/circuit_escape/graph/graph.hpp"
#include "../include/circuit_escape/grid.hpp"
#include "../include/circuit_escape/position.hpp"

using namespace circuit_escape;

namespace {

void disjointSetBasic() {
    DisjointSet<int> ds;
    ds.makeSet(5);
    assert(ds.size() == 5);
    // Cada elemento es su propio representante
    for (int i = 0; i < 5; ++i) {
        assert(ds.find(i) == i);
    }
    // union de conjuntos distintos devuelve true
    assert(ds.unionSets(0, 1));
    assert(ds.find(0) == ds.find(1));
    // union repetida devuelve false
    assert(!ds.unionSets(0, 1));
    // union de otro par
    assert(ds.unionSets(2, 3));
    assert(ds.find(2) == ds.find(3));
    assert(ds.find(0) != ds.find(2));
    // unir los dos grupos
    assert(ds.unionSets(1, 3));
    assert(ds.find(0) == ds.find(2));
}

void graphFromGrid() {
    // Grid 3x3: centro libre, resto paredes
    Grid<Cell> grid(3, 3);
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c)
            grid.at({r, c}) = Wall{};
    grid.at({1, 1}) = Empty{};

    Graph g(grid);
    assert(g.vertexCount() == 1);
    assert(g.neighbors({1, 1}).empty());

    // Grid 3x3: todos libres
    Grid<Cell> grid2(3, 3);
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c)
            grid2.at({r, c}) = Empty{};

    Graph g2(grid2);
    assert(g2.vertexCount() == 9);
    // (0,0) tiene vecinos: (1,0) y (0,1)
    auto n00 = g2.neighbors({0, 0});
    assert(n00.size() == 2);
    // (1,1) tiene 4 vecinos
    auto n11 = g2.neighbors({1, 1});
    assert(n11.size() == 4);
}

void graphRespectsWalls() {
    // Grid con un muro en el centro
    Grid<Cell> grid(3, 3);
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c)
            grid.at({r, c}) = Empty{};
    grid.at({1, 1}) = Wall{};

    Graph g(grid);
    assert(g.vertexCount() == 8);
    // (0,1) no puede ir a (1,1) porque es muro
    auto n = g.neighbors({0, 1});
    // (0,1) neighbors: (0,0), (0,2), (1,1 is wall so skip)
    // Actually neighbor positions: up (invalid), down (1,1 wall), left (0,0), right (0,2)
    assert(n.size() == 2);
}

void graphIndexOf() {
    Grid<Cell> grid(2, 2);
    for (std::size_t r = 0; r < 2; ++r)
        for (std::size_t c = 0; c < 2; ++c)
            grid.at({r, c}) = Empty{};

    Graph g(grid);
    assert(g.indexOf({0, 0}) >= 0);
    assert(g.indexOf({1, 1}) >= 0);
    assert(g.indexOf({5, 5}) == -1);  // no existe
}

}  // namespace

int main() {
    disjointSetBasic();
    graphFromGrid();
    graphRespectsWalls();
    graphIndexOf();
    return 0;
}