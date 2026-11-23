#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <vector>

#include "circuit_escape/map_generation/map_selector.hpp"

using namespace circuit_escape;

namespace {

LevelSelection select(std::vector<MapCandidate> c) { return select_levels(c); }

// Ejemplo del enunciado: costos 10, 18 y 32; promedio 20.
void statementExample() {
    const LevelSelection s = select({{1, 10}, {2, 18}, {3, 32}});
    assert(s.ok());
    assert(s.average == 20.0);
    assert(s.easy == 1 && s.medium == 2 && s.hard == 3);
}

// Promedio 20 con intermedios 18 y 21: gana 21 (a 1 del promedio, 18 a 2).
void mediumClosestToAverage() {
    const LevelSelection s = select({{1, 10}, {2, 18}, {3, 21}, {4, 31}});
    assert(s.ok() && s.average == 20.0);
    assert(s.medium == 3);
}

// "Si los mapas 2 y 5 cuestan 18 ... elegir el mapa 2".
void tieOnCostPicksLowerId() {
    const LevelSelection s = select({{5, 18}, {1, 10}, {2, 18}, {3, 32}});
    assert(s.medium == 2);
    const LevelSelection e = select({{3, 10}, {1, 10}, {2, 20}, {4, 30}, {6, 30}});
    assert(e.easy == 1);
    assert(e.hard == 4);
}

// Intermedios 18 y 22 a igual distancia del promedio 20: id menor.
void tieOnDistancePicksLowerId() {
    const LevelSelection s = select({{1, 10}, {2, 22}, {3, 18}, {4, 30}});
    assert(s.average == 20.0);
    assert(s.medium == 2);
    const LevelSelection swapped = select({{1, 10}, {2, 18}, {3, 22}, {4, 30}});
    assert(swapped.medium == 2);
}

// El medio nunca es el minimo ni el maximo, aunque este mas cerca del promedio.
void mediumExcludesExtremes() {
    const LevelSelection s = select({{1, 19}, {2, 19}, {3, 5}, {4, 40}, {5, 37}});
    // Promedio 24; minimo 5 (id 3) y maximo 40 (id 4).
    assert(s.easy == 3 && s.hard == 4);
    assert(s.medium == 1);  // 19 esta a 5 del promedio, 37 a 13
    const LevelSelection t = select({{1, 20}, {2, 20}, {3, 10}, {4, 30}});
    assert(t.medium == 1);
}

void notEnoughDistinctCosts() {
    const LevelSelection two = select({{1, 10}, {2, 10}, {3, 20}});
    assert(!two.ok());
    assert(two.distinct_costs == 2);
    assert(!two.easy && !two.medium && !two.hard);
    const LevelSelection one = select({{1, 15}});
    assert(!one.ok());
}

void emptyBatch() {
    const LevelSelection s = select({});
    assert(!s.ok());
    assert(!s.easy);
}

// El orden de llegada (que trabajador termina primero) no cambia el resultado.
void orderIndependent() {
    std::vector<MapCandidate> c = {{1, 40}, {2, 18}, {3, 25}, {4, 18}, {5, 9}, {6, 31}};
    const LevelSelection base = select_levels(c);
    assert(base.easy == 5 && base.hard == 1);
    // Las 720 permutaciones dan el mismo resultado.
    const auto byId = [](const MapCandidate& a, const MapCandidate& b) { return a.id < b.id; };
    std::sort(c.begin(), c.end(), byId);
    int permutations = 0;
    do {
        ++permutations;
        const LevelSelection other = select_levels(c);
        assert(other.easy == base.easy && other.medium == base.medium &&
               other.hard == base.hard);
    } while (std::next_permutation(c.begin(), c.end(), byId));
    assert(permutations == 720);
}

}  // namespace

int main() {
    statementExample();
    mediumClosestToAverage();
    tieOnCostPicksLowerId();
    tieOnDistancePicksLowerId();
    mediumExcludesExtremes();
    notEnoughDistinctCosts();
    emptyBatch();
    orderIndependent();
    return 0;
}
