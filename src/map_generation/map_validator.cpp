#include "../../include/circuit_escape/map_generation/map_validator.hpp"

#include <variant>

#include "../../include/circuit_escape/graph/graph.hpp"

namespace circuit_escape {

std::string toString(ValidationStatus status) {
    switch (status) {
        case ValidationStatus::accepted: return "aceptado";
        case ValidationStatus::needs_recharge_proof: return "requiere recargas";
        case ValidationStatus::rejected: return "rechazado";
    }
    return "?";
}

MapValidation validate_map(const Map& map) {
    MapValidation v;
    v.map_id = map.id;

    // Cada validacion tiene su propio grafo, distancias y cola de prioridad:
    // es segura en paralelo sobre mapas distintos.
    const Graph graph(map.grid, map.rules);
    v.routes = plan_routes(graph, RouteQuery{map.start, map.switchA, map.switchB, map.exit});

    // Reutiliza countCells (utilidad generica sobre rangos del Proyecto 1).
    v.batteries = countCells(map.grid.begin(), map.grid.end(), [](const Cell& cell) {
        const auto* battery = std::get_if<Battery>(&cell);
        return battery && !battery->consumed;
    });

    const RoutePlan* best = v.routes.best_plan();
    if (!best) {
        for (RouteTarget t : {RouteTarget::switch_a, RouteTarget::switch_b, RouteTarget::exit}) {
            if (!v.routes.cost_from_origin[static_cast<std::size_t>(t)]) v.unreachable.push_back(t);
        }
        v.status = ValidationStatus::rejected;
        v.diagnosis = "Ningun recorrido completo es posible";
        if (!v.unreachable.empty()) {
            v.diagnosis += "; objetivo inaccesible:";
            for (RouteTarget t : v.unreachable) v.diagnosis += " " + toString(t);
        }
        return v;
    }

    v.min_cost = best->total;
    v.energy = energy_verdict(map.rules.initialEnergy, best->total);
    const std::string summary = "recorrido " + best->describe() + ", costo " +
                                std::to_string(best->total) + ", energia " +
                                std::to_string(map.rules.initialEnergy);

    if (*v.energy == EnergyVerdict::enough) {
        v.status = ValidationStatus::accepted;
        v.diagnosis = summary + ": " + toString(*v.energy);
    } else if (v.batteries == 0) {
        v.status = ValidationStatus::rejected;
        v.diagnosis = summary + ": " + toString(*v.energy) + " y no hay baterias";
    } else {
        v.status = ValidationStatus::needs_recharge_proof;
        v.diagnosis = summary + ": " + toString(*v.energy) + "; hay " +
                      std::to_string(v.batteries) +
                      " baterias, se exige una partida ganadora que las use";
    }
    return v;
}

}  // namespace circuit_escape
