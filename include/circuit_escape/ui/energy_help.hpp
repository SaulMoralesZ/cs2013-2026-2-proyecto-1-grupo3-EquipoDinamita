#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../environment.hpp"
#include "../graph/graph.hpp"
#include "../pathfinding/route_planner.hpp"

namespace circuit_escape {

// Ayuda de energia (seccion 3.3). No depende de FTXUI: se calcula y se
// formatea como texto; la consola solo la dibuja.

struct HelpRow {
    std::string objective;  // "A", "B", "Salida"
    std::string status;     // Pendiente / Activado / Requiere A y B / Habilitada
    std::string cost;       // gasto desde la posicion actual, "—" o "inaccesible"
};

// Que camino resalta el tablero de la ayuda.
enum class HelpHighlight { route, to_a, to_b };

struct EnergyHelp {
    std::vector<HelpRow> rows;
    bool route_found{false};
    std::string route_text;  // "posicion actual -> B -> A -> salida"
    int energy{0};
    int total{0};
    std::optional<EnergyVerdict> verdict;  // solo si hay recorrido
    std::size_t turn{0};                   // turno en que se calculo
    std::size_t dijkstra_runs{0};
    // Caminos para resaltar (vacios si no aplica o no hay camino).
    std::vector<Position> route_path;
    std::vector<Position> path_to_a;
    std::vector<Position> path_to_b;

    [[nodiscard]] const std::vector<Position>& path(HelpHighlight highlight) const;
};

// Calcula la ayuda desde la posicion, energia e interruptores ACTUALES.
// Solo lee: no avanza turnos ni mueve al vigilante. Reutiliza plan_routes,
// la misma implementacion de Dijkstra de la validacion de mapas.
// El grafo debe construirse con el grid y las reglas de la partida.
[[nodiscard]] EnergyHelp compute_energy_help(const NavigationEnvironment& environment,
                                             const Graph& graph);

// Texto de la ayuda, linea por linea (tabla, recorrido, saldo y estado).
[[nodiscard]] std::vector<std::string> format_energy_help(const EnergyHelp& help);

}  // namespace circuit_escape
