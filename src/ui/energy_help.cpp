#include "../../include/circuit_escape/ui/energy_help.hpp"

#include <string>

namespace circuit_escape {

namespace {

constexpr const char* kDash = "\xE2\x80\x94";  // "—"

std::string pad(std::string text, std::size_t width) {
    // Ancho visible: cuenta caracteres UTF-8, no bytes.
    std::size_t visible = 0;
    for (unsigned char ch : text) visible += (ch & 0xC0) != 0x80;
    if (visible < width) text.append(width - visible, ' ');
    return text;
}

std::string cost_text(const std::optional<int>& cost) {
    return cost ? std::to_string(*cost) : std::string("inaccesible");
}

std::string verdict_text(EnergyVerdict verdict) {
    switch (verdict) {
        case EnergyVerdict::enough: return "ALCANZA PARA ESTE RECORRIDO SIN VIGILANTE";
        case EnergyVerdict::exactly_zero: return "SE AGOTARIA AL LLEGAR: NO PERMITE GANAR";
        case EnergyVerdict::insufficient: return "NO ALCANZA PARA ESTE RECORRIDO SIN RECARGAS";
    }
    return "";
}

}  // namespace

const std::vector<Position>& EnergyHelp::path(HelpHighlight highlight) const {
    switch (highlight) {
        case HelpHighlight::to_a: return path_to_a;
        case HelpHighlight::to_b: return path_to_b;
        case HelpHighlight::route: break;
    }
    return route_path;
}

EnergyHelp compute_energy_help(const NavigationEnvironment& environment, const Graph& graph) {
    EnergyHelp help;
    help.energy = environment.energy();
    help.turn = environment.turn();

    const bool objectives = environment.has_objectives();
    const bool aActive = !objectives || environment.is_switch_active(SwitchId::a);
    const bool bActive = !objectives || environment.is_switch_active(SwitchId::b);

    RouteQuery query{environment.player_position(), std::nullopt, std::nullopt,
                     environment.goal()};
    if (!aActive) query.switch_a = environment.switch_position(SwitchId::a);
    if (!bActive) query.switch_b = environment.switch_position(SwitchId::b);
    const RoutePlanning plan = plan_routes(graph, query);
    help.dijkstra_runs = plan.dijkstra_runs;

    const auto at = [&](RouteTarget t) { return static_cast<std::size_t>(t); };

    // Filas de interruptores: Pendiente + gasto, o Activado + "—".
    if (objectives) {
        for (RouteTarget t : {RouteTarget::switch_a, RouteTarget::switch_b}) {
            const bool active = t == RouteTarget::switch_a ? aActive : bActive;
            help.rows.push_back({toString(t), active ? "Activado" : "Pendiente",
                                 active ? kDash : cost_text(plan.cost_from_origin[at(t)])});
        }
        help.path_to_a = plan.path_from_origin[at(RouteTarget::switch_a)];
        help.path_to_b = plan.path_from_origin[at(RouteTarget::switch_b)];
    }

    // Fila de la salida: bloqueada hasta activar ambos interruptores.
    if (!aActive || !bActive) {
        std::string status = "Requiere ";
        if (!aActive && !bActive) status += "A y B";
        else status += !aActive ? "A" : "B";
        help.rows.push_back({"Salida", status, kDash});
    } else {
        help.rows.push_back(
            {"Salida", "Habilitada", cost_text(plan.cost_from_origin[at(RouteTarget::exit)])});
    }

    // Recorrido completo de menor gasto.
    if (const RoutePlan* best = plan.best_plan()) {
        help.route_found = true;
        help.total = best->total;
        help.verdict = energy_verdict(help.energy, help.total);
        help.route_path = best->full_path();
        help.route_text = "posicion actual";
        for (RouteTarget t : best->order) help.route_text += " -> " + toString(t);
    }
    return help;
}

std::vector<std::string> format_energy_help(const EnergyHelp& help) {
    std::vector<std::string> lines;
    lines.push_back(pad("Objetivo", 10) + pad("Estado", 18) + "Gasto desde aqui");
    for (const HelpRow& row : help.rows) {
        lines.push_back(pad(row.objective, 10) + pad(row.status, 18) + row.cost);
    }
    lines.emplace_back();

    if (!help.route_found) {
        // Sin camino: no se calcula saldo.
        lines.push_back("Recorrido: Destino inaccesible");
        lines.push_back("Energia disponible: " + std::to_string(help.energy));
        return lines;
    }
    lines.push_back("Recorrido: " + help.route_text);
    const int left = help.energy - help.total;
    lines.push_back("Energia disponible: " + std::to_string(help.energy) +
                    "   Gasto total: " + std::to_string(help.total) + "   " +
                    (left >= 0 ? "Quedarian: " + std::to_string(left)
                               : "Faltarian: " + std::to_string(-left)));
    lines.push_back("Estado: " + verdict_text(*help.verdict));
    return lines;
}

}  // namespace circuit_escape
