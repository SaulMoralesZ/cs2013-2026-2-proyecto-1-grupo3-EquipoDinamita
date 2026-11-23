#include "../../include/circuit_escape/map_generation/map_selector.hpp"

#include <algorithm>
#include <cstdint>
#include <set>

namespace circuit_escape {

LevelSelection select_levels(std::span<const MapCandidate> candidates) {
    LevelSelection selection;
    if (candidates.empty()) {
        selection.error = "No hay mapas con victoria comprobada: se detiene la preparacion.";
        return selection;
    }

    std::int64_t sum = 0;
    std::set<int> costs;
    for (const MapCandidate& c : candidates) {
        sum += c.min_cost;
        costs.insert(c.min_cost);
    }
    const auto n = static_cast<std::int64_t>(candidates.size());
    selection.average = static_cast<double>(sum) / static_cast<double>(n);
    selection.distinct_costs = costs.size();

    if (costs.size() < 3) {
        selection.error = "El lote no permite ofrecer los tres niveles: solo hay " +
                          std::to_string(costs.size()) +
                          " costo(s) distinto(s). Genere otro lote con otra semilla o "
                          "configuracion.";
        return selection;
    }

    const int minCost = *costs.begin();
    const int maxCost = *costs.rbegin();

    // Candidato preferido segun una clave; empate -> id menor.
    const auto pick = [&](auto better) {
        const MapCandidate* chosen = nullptr;
        for (const MapCandidate& c : candidates) {
            if (!chosen || better(c, *chosen) ||
                (!better(*chosen, c) && c.id < chosen->id)) {
                chosen = &c;
            }
        }
        return chosen->id;
    };

    selection.easy = pick([](const MapCandidate& a, const MapCandidate& b) {
        return a.min_cost < b.min_cost;
    });
    selection.hard = pick([](const MapCandidate& a, const MapCandidate& b) {
        return a.min_cost > b.min_cost;
    });

    // Medio: solo costos distintos del minimo y del maximo. La distancia al
    // promedio se compara en enteros: |costo - suma/n| ~ |costo*n - suma|.
    const auto distance = [&](const MapCandidate& c) {
        const std::int64_t d = static_cast<std::int64_t>(c.min_cost) * n - sum;
        return d < 0 ? -d : d;
    };
    const MapCandidate* medium = nullptr;
    for (const MapCandidate& c : candidates) {
        if (c.min_cost == minCost || c.min_cost == maxCost) continue;
        if (!medium || distance(c) < distance(*medium) ||
            (distance(c) == distance(*medium) && c.id < medium->id)) {
            medium = &c;
        }
    }
    selection.medium = medium->id;  // existe: hay al menos tres costos distintos
    return selection;
}

}  // namespace circuit_escape
