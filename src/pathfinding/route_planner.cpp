#include "../../include/circuit_escape/pathfinding/route_planner.hpp"

#include <utility>

#include "../../include/circuit_escape/pathfinding/dijkstra.hpp"

namespace circuit_escape {

std::string toString(RouteTarget target) {
    switch (target) {
        case RouteTarget::switch_a: return "A";
        case RouteTarget::switch_b: return "B";
        case RouteTarget::exit: return "salida";
    }
    return "?";
}

std::string toString(EnergyVerdict verdict) {
    switch (verdict) {
        case EnergyVerdict::enough: return "alcanza y queda energia";
        case EnergyVerdict::exactly_zero: return "se agotaria al llegar (energia cero no gana)";
        case EnergyVerdict::insufficient: return "no alcanza sin recargas";
    }
    return "?";
}

std::string RoutePlan::describe() const {
    std::string text = "origen";
    for (RouteTarget t : order) text += " -> " + toString(t);
    return text;
}

std::vector<Position> RoutePlan::full_path() const {
    std::vector<Position> path;
    for (const RouteLeg& leg : legs) {
        if (leg.path.empty()) return {};
        // La primera casilla de cada tramo es la ultima del anterior.
        const std::size_t skip = path.empty() ? 0 : 1;
        path.insert(path.end(), leg.path.begin() + static_cast<std::ptrdiff_t>(skip),
                    leg.path.end());
    }
    return path;
}

namespace {

std::size_t slot(RouteTarget t) { return static_cast<std::size_t>(t); }

}  // namespace

RoutePlanning plan_routes(const Graph& graph, const RouteQuery& query) {
    RoutePlanning planning;

    // Posicion de cada objetivo pendiente (la salida siempre esta pendiente).
    std::array<std::optional<Position>, 3> where{query.switch_a, query.switch_b, query.exit};

    // Dijkstra desde el origen y desde cada interruptor pendiente, en orden.
    // Un interruptor que no es transitable no tiene arbol (todo inaccesible).
    const auto run = [&](Position source) -> std::optional<ShortestPaths> {
        if (graph.indexOf(source) < 0) return std::nullopt;
        ++planning.dijkstra_runs;
        return dijkstra(graph, source);
    };
    const std::optional<ShortestPaths> fromOrigin = run(query.origin);
    std::array<std::optional<ShortestPaths>, 2> fromSwitch;
    for (RouteTarget t : {RouteTarget::switch_a, RouteTarget::switch_b}) {
        if (where[slot(t)]) fromSwitch[slot(t)] = run(*where[slot(t)]);
    }

    for (RouteTarget t : {RouteTarget::switch_a, RouteTarget::switch_b, RouteTarget::exit}) {
        if (!where[slot(t)] || !fromOrigin) continue;
        planning.cost_from_origin[slot(t)] = fromOrigin->cost_to(*where[slot(t)]);
        planning.path_from_origin[slot(t)] = fromOrigin->path_to(*where[slot(t)]).path;
    }

    // Ordenes candidatos (orden fijo para el desempate).
    std::vector<std::vector<RouteTarget>> orders;
    const bool a = where[0].has_value();
    const bool b = where[1].has_value();
    if (a && b) {
        orders.push_back({RouteTarget::switch_a, RouteTarget::switch_b, RouteTarget::exit});
        orders.push_back({RouteTarget::switch_b, RouteTarget::switch_a, RouteTarget::exit});
    } else if (a) {
        orders.push_back({RouteTarget::switch_a, RouteTarget::exit});
    } else if (b) {
        orders.push_back({RouteTarget::switch_b, RouteTarget::exit});
    } else {
        orders.push_back({RouteTarget::exit});
    }

    for (auto& order : orders) {
        RoutePlan plan;
        plan.order = order;
        plan.viable = true;
        Position from = query.origin;
        const ShortestPaths* tree = fromOrigin ? &*fromOrigin : nullptr;
        for (RouteTarget t : order) {
            const Position to = *where[slot(t)];
            RouteLeg leg{from, to, t, std::nullopt, {}};
            if (tree) {
                leg.cost = tree->cost_to(to);
                if (leg.cost) leg.path = tree->path_to(to).path;
            }
            if (leg.cost) {
                plan.total += *leg.cost;
            } else {
                plan.viable = false;
            }
            plan.legs.push_back(std::move(leg));
            from = to;
            tree = (t == RouteTarget::exit || !fromSwitch[slot(t)]) ? nullptr
                                                                    : &*fromSwitch[slot(t)];
        }
        if (!plan.viable) plan.total = 0;
        planning.candidates.push_back(std::move(plan));
    }

    for (std::size_t i = 0; i < planning.candidates.size(); ++i) {
        const RoutePlan& plan = planning.candidates[i];
        if (!plan.viable) continue;
        if (!planning.best || plan.total < planning.candidates[*planning.best].total) {
            planning.best = i;
        }
    }
    return planning;
}

}  // namespace circuit_escape
