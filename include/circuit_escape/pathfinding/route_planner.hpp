#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../graph/graph.hpp"
#include "../position.hpp"

namespace circuit_escape {

// Objetivos de un recorrido completo.
enum class RouteTarget { switch_a, switch_b, exit };

[[nodiscard]] std::string toString(RouteTarget target);

// Consulta de recorrido (Apendice A). switch_a / switch_b = nullopt cuando ese
// interruptor ya esta activado y se omite.
//  - Validacion (seccion 2.3): origin = inicio, A y B pendientes.
//  - Ayuda H (seccion 3.3): origin = posicion actual del jugador.
struct RouteQuery {
    Position origin;
    std::optional<Position> switch_a;
    std::optional<Position> switch_b;
    Position exit;
};

// Un tramo del recorrido: menor gasto entre dos casillas consecutivas.
struct RouteLeg {
    Position from;
    Position to;
    RouteTarget target;
    std::optional<int> cost;      // nullopt = tramo sin camino
    std::vector<Position> path;   // vacio si no hay camino
};

// Un orden concreto de visita (p. ej. origen -> B -> A -> salida).
struct RoutePlan {
    std::vector<RouteTarget> order;  // objetivos en orden; el ultimo es la salida
    std::vector<RouteLeg> legs;
    bool viable{false};              // todos los tramos tienen camino
    int total{0};                    // suma de tramos (solo si viable)

    [[nodiscard]] std::string describe() const;  // "posicion -> B -> A -> salida"
    // Camino completo concatenando los tramos (sin repetir las uniones).
    [[nodiscard]] std::vector<Position> full_path() const;
};

struct RoutePlanning {
    // Candidatos en orden fijo: primero A antes que B, luego B antes que A.
    std::vector<RoutePlan> candidates;
    // Indice del candidato viable de menor total; empate -> el primero.
    std::optional<std::size_t> best;
    // Gasto minimo desde el ORIGEN hacia cada objetivo (no se suman entre si):
    // indice = RouteTarget. nullopt si es inaccesible.
    std::array<std::optional<int>, 3> cost_from_origin{};
    // Caminos de menor gasto desde el origen hacia cada objetivo (para resaltar).
    std::array<std::vector<Position>, 3> path_from_origin{};
    std::size_t dijkstra_runs{0};

    [[nodiscard]] const RoutePlan* best_plan() const {
        return best ? &candidates[*best] : nullptr;
    }
};

// Ejecuta Dijkstra en orden (no en paralelo) desde el origen y desde cada
// interruptor pendiente, y arma los recorridos completos. Reutiliza la misma
// implementacion de Dijkstra que la ayuda H.
[[nodiscard]] RoutePlanning plan_routes(const Graph& graph, const RouteQuery& query);

// Diagnostico de energia frente a un gasto (secciones 2.3 y 3.3).
enum class EnergyVerdict {
    enough,        // energia > gasto: alcanza y queda energia al escapar
    exactly_zero,  // energia == gasto: se agotaria al llegar, no permite ganar
    insufficient   // energia < gasto: no alcanza sin recargas
};

[[nodiscard]] constexpr EnergyVerdict energy_verdict(int energy, int cost) noexcept {
    if (energy > cost) return EnergyVerdict::enough;
    if (energy == cost) return EnergyVerdict::exactly_zero;
    return EnergyVerdict::insufficient;
}

[[nodiscard]] std::string toString(EnergyVerdict verdict);

}  // namespace circuit_escape
