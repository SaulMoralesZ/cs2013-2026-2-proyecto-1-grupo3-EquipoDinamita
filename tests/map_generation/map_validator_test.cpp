#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "circuit_escape/graph/graph.hpp"
#include "circuit_escape/map_generation/map_builder.hpp"
#include "circuit_escape/map_generation/map_validator.hpp"
#include "circuit_escape/pathfinding/route_planner.hpp"

using namespace circuit_escape;

namespace {

// Mapa de la seccion 3.3: '3' = terreno aspero de costo 3, letras y '1' = costo 1.
//   #########
//   #I333A1S#
//   #1###1#1#
//   #B111V11#
//   #########
Map exampleMap(int energy) {
    const std::vector<std::string> rows = {
        "#########", "#I333A1S#", "#1###1#1#", "#B111V11#", "#########",
    };
    Map map;
    map.id = 7;
    map.grid = Grid<Cell>(rows.size(), rows.front().size());
    for (std::size_t r = 0; r < rows.size(); ++r)
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const char ch = rows[r][c];
            map.grid.at({r, c}) = ch == '#'   ? Cell{Wall{}}
                                  : ch == '3' ? Cell{RoughTerrain{}}
                                  : ch == 'S' ? Cell{Exit{}}
                                              : Cell{Empty{}};
        }
    map.start = {1, 1};
    map.switchA = {1, 5};
    map.switchB = {3, 1};
    map.exit = {1, 7};
    map.vigilanteStart = {3, 5};
    map.rules.baseCost = 1;
    map.rules.roughCost = 3;
    map.rules.initialEnergy = energy;
    map.rules.maximumEnergy = energy;
    return map;
}

constexpr std::size_t kA = 0, kB = 1, kExit = 2;

void bothRoutesOfTheStatementExample() {
    const MapValidation v = validate_map(exampleMap(12));
    assert(v.map_id == 7);
    assert(v.routes.dijkstra_runs == 3);  // inicio, A y B
    assert(v.routes.candidates.size() == 2);

    const RoutePlan& ab = v.routes.candidates[0];
    const RoutePlan& ba = v.routes.candidates[1];
    assert((ab.order == std::vector<RouteTarget>{RouteTarget::switch_a, RouteTarget::switch_b,
                                                 RouteTarget::exit}));
    assert(ab.viable && ab.total == 22);  // 8 + 6 + 8
    assert(ba.viable && ba.total == 10);  // 2 + 6 + 2
    assert(*ba.legs[0].cost == 2 && *ba.legs[1].cost == 6 && *ba.legs[2].cost == 2);

    assert(v.routes.best == 1);
    assert(v.min_cost == 10);
    // Gastos desde el inicio hacia cada objetivo (no se suman).
    assert(v.routes.cost_from_origin[kA] == 8);
    assert(v.routes.cost_from_origin[kB] == 2);

    // El camino completo encadena los tramos sin repetir las uniones.
    const auto path = ba.full_path();
    assert(path.front() == Position(1, 1) && path.back() == Position(1, 7));
    for (std::size_t i = 1; i < path.size(); ++i) assert(manhattanDistance(path[i - 1], path[i]) == 1);
}

// Apendice A: con 12 quedan 2; con 10 se llegaria a cero; con 9 faltan recargas.
void energyDiagnosis() {
    const MapValidation enough = validate_map(exampleMap(12));
    assert(enough.status == ValidationStatus::accepted);
    assert(enough.energy == EnergyVerdict::enough);

    const MapValidation zero = validate_map(exampleMap(10));
    assert(zero.energy == EnergyVerdict::exactly_zero);
    assert(zero.status == ValidationStatus::rejected);  // sin baterias
    assert(!zero.usable());

    const MapValidation short9 = validate_map(exampleMap(9));
    assert(short9.energy == EnergyVerdict::insufficient);
    assert(short9.status == ValidationStatus::rejected);
    assert(short9.min_cost == 10);  // el costo se informa aunque se rechace
}

void rechargesRequireProof() {
    Map map = exampleMap(9);
    map.grid.at({3, 3}) = Battery{};  // una bateria en el camino
    const MapValidation v = validate_map(map);
    assert(v.batteries == 1);
    assert(v.status == ValidationStatus::needs_recharge_proof);
    assert(v.usable());
    assert(v.min_cost == 10);  // la recarga no es un peso negativo
}

void inaccessibleObjectives() {
    {   // A encerrado por muros
        Map map = exampleMap(50);
        map.grid.at({1, 4}) = Wall{};
        map.grid.at({1, 6}) = Wall{};
        map.grid.at({2, 5}) = Wall{};
        const MapValidation v = validate_map(map);
        assert(v.status == ValidationStatus::rejected);
        assert(!v.routes.best && !v.min_cost);
        assert(!v.routes.candidates[0].viable && !v.routes.candidates[1].viable);
        assert((v.unreachable == std::vector<RouteTarget>{RouteTarget::switch_a}));
        assert(v.diagnosis.find("inaccesible") != std::string::npos);
    }
    {   // salida encerrada
        Map map = exampleMap(50);
        map.grid.at({1, 6}) = Wall{};
        map.grid.at({2, 7}) = Wall{};
        const MapValidation v = validate_map(map);
        assert(v.status == ValidationStatus::rejected);
        assert((v.unreachable == std::vector<RouteTarget>{RouteTarget::exit}));
    }
}

void validationDoesNotModifyTheMap() {
    const Map map = exampleMap(12);
    const Map copy = map;
    (void)validate_map(map);
    for (std::size_t r = 0; r < map.grid.rows(); ++r)
        for (std::size_t c = 0; c < map.grid.columns(); ++c)
            assert(map.grid.at({r, c}).index() == copy.grid.at({r, c}).index());
}

// Planificador con interruptores ya activados (lo reutilizara la ayuda H).
void plannerSkipsActiveSwitches() {
    const Map map = exampleMap(12);
    const Graph graph(map.grid, map.rules);

    // Solo B pendiente, desde A: A -> B -> salida = 6 + 8.
    const RoutePlanning onlyB =
        plan_routes(graph, RouteQuery{map.switchA, std::nullopt, map.switchB, map.exit});
    assert(onlyB.candidates.size() == 1);
    assert(onlyB.dijkstra_runs == 2);
    assert(onlyB.best_plan()->total == 14);
    assert(!onlyB.cost_from_origin[kA].has_value());  // activado: sin gasto

    // Ambos activos, en A: directo a la salida = 2 (ejemplo "Salida Habilitada 2").
    const RoutePlanning none =
        plan_routes(graph, RouteQuery{map.switchA, std::nullopt, std::nullopt, map.exit});
    assert(none.candidates.size() == 1);
    assert(none.dijkstra_runs == 1);
    assert(none.best_plan()->total == 2);
    assert(none.cost_from_origin[kExit] == 2);
}

void generatedMapsAreValidatedReproducibly() {
    MapConfig config;
    config.numBatteries = 2;
    for (unsigned seed = 1; seed <= 5; ++seed) {
        const Map map = MapBuilder::buildMap(1, seed, 20, 30, config);
        const MapValidation first = validate_map(map);
        const MapValidation again = validate_map(map);
        // Kruskal conecta todas las casillas: siempre hay recorrido.
        assert(first.min_cost.has_value());
        assert(first.unreachable.empty());
        assert(first.min_cost == again.min_cost);
        assert(first.routes.best == again.routes.best);
        assert(first.routes.best_plan()->full_path() == again.routes.best_plan()->full_path());
    }
}

}  // namespace

int main() {
    bothRoutesOfTheStatementExample();
    energyDiagnosis();
    rechargesRequireProof();
    inaccessibleObjectives();
    validationDoesNotModifyTheMap();
    plannerSkipsActiveSwitches();
    generatedMapsAreValidatedReproducibly();
    return 0;
}
