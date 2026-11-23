#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "circuit_escape/environment.hpp"
#include "circuit_escape/graph/graph.hpp"
#include "circuit_escape/ui/energy_help.hpp"

using namespace circuit_escape;

namespace {

// '#' muro, '3' terreno aspero, 'S' salida; el resto libre (costo 1).
Scenario scenarioFrom(const std::vector<std::string>& rows, Position start, Position a,
                      Position b, Position guard, int energy, int rough = 3) {
    Scenario s{Grid<Cell>(rows.size(), rows.front().size())};
    for (std::size_t r = 0; r < rows.size(); ++r)
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const char ch = rows[r][c];
            s.grid.at({r, c}) = ch == '#'   ? Cell{Wall{}}
                                : ch == '3' ? Cell{RoughTerrain{}}
                                : ch == 'S' ? Cell{Exit{}}
                                            : Cell{Empty{}};
        }
    s.start = start;
    s.switch_a = a;
    s.switch_b = b;
    s.guard_start = guard;
    s.rules = rulesFor(Difficulty::standard);
    s.rules.baseCost = 1;
    s.rules.roughCost = rough;
    s.rules.initialEnergy = energy;
    s.rules.maximumEnergy = energy;
    return s;
}

// Mapa de la seccion 3.3.
Scenario statementExample(int energy) {
    return scenarioFrom({"#########", "#I333A1S#", "#1###1#1#", "#B111V11#", "#########"},
                        {1, 1}, {1, 5}, {3, 1}, {3, 5}, energy);
}

EnergyHelp helpFor(const NavigationEnvironment& env) {
    const Graph graph(env.grid(), env.rules());
    return compute_energy_help(env, graph);
}

void statementTable() {
    const NavigationEnvironment env(statementExample(12));
    const EnergyHelp help = helpFor(env);
    assert(help.dijkstra_runs == 3);  // posicion actual, A y B
    const std::vector<std::string> expected = {
        "Objetivo  Estado            Gasto desde aqui",
        "A         Pendiente         8",
        "B         Pendiente         2",
        "Salida    Requiere A y B    \xE2\x80\x94",
        "",
        "Recorrido: posicion actual -> B -> A -> salida",
        "Energia disponible: 12   Gasto total: 10   Quedarian: 2",
        "Estado: ALCANZA PARA ESTE RECORRIDO SIN VIGILANTE",
    };
    assert(format_energy_help(help) == expected);
    assert((help.path_to_b == std::vector<Position>{{1, 1}, {2, 1}, {3, 1}}));
    assert(help.route_path.front() == Position(1, 1) && help.route_path.back() == Position(1, 7));
    assert(help.path(HelpHighlight::to_a) == help.path_to_a);
    assert(help.path(HelpHighlight::route) == help.route_path);
}

// Energia suficiente, cero e insuficiente.
void energyVerdicts() {
    const auto line = [](int energy, std::size_t i) {
        const NavigationEnvironment env(statementExample(energy));
        return format_energy_help(helpFor(env))[i];
    };
    assert(line(10, 6) == "Energia disponible: 10   Gasto total: 10   Quedarian: 0");
    assert(line(10, 7) == "Estado: SE AGOTARIA AL LLEGAR: NO PERMITE GANAR");
    assert(line(9, 6) == "Energia disponible: 9   Gasto total: 10   Faltarian: 1");
    assert(line(9, 7) == "Estado: NO ALCANZA PARA ESTE RECORRIDO SIN RECARGAS");
}

// Consultar la ayuda no avanza turnos ni mueve al vigilante.
void helpDoesNotPlay() {
    const NavigationEnvironment env(statementExample(12));
    const auto before = env.state();
    (void)helpFor(env);
    (void)helpFor(env);
    const auto after = env.state();
    assert(after.turn == before.turn && after.energy == before.energy);
    assert(after.agent == before.agent && after.guard == before.guard);
}

// Tras moverse y activar B se recalcula desde la nueva posicion y energia.
void updatesAfterMovingAndActivating() {
    NavigationEnvironment env(statementExample(12));
    (void)env.step(Action::down);  // (2,1)
    (void)env.step(Action::down);  // (3,1) = B
    assert(env.is_switch_active(SwitchId::b));
    const EnergyHelp help = helpFor(env);
    assert(help.turn == 2);
    assert(help.energy == 10);
    assert(help.rows[1].status == "Activado" && help.rows[1].cost == "\xE2\x80\x94");
    assert(help.rows[0].status == "Pendiente" && help.rows[0].cost == "6");
    assert(help.rows[2].status == "Requiere A");
    assert(help.route_text == "posicion actual -> A -> salida");
    assert(help.total == 8);  // B ya no se cuenta
    assert(help.path_to_b.empty());
    assert(help.dijkstra_runs == 2);
}

// Con A y B activos: "Salida Habilitada" y su gasto desde aqui.
void exitEnabled() {
    // Pasillo con el vigilante detras: #G@AB....S#
    NavigationEnvironment env(scenarioFrom({"###########", "#........S#", "###########"},
                                           {1, 2}, {1, 3}, {1, 4}, {1, 1}, 60));
    (void)env.step(Action::right);
    (void)env.step(Action::right);
    const EnergyHelp help = helpFor(env);
    const auto lines = format_energy_help(help);
    assert(lines[1] == "A         Activado          \xE2\x80\x94");
    assert(lines[2] == "B         Activado          \xE2\x80\x94");
    assert(lines[3] == "Salida    Habilitada        5");
    assert(help.route_text == "posicion actual -> salida");
    assert(help.dijkstra_runs == 1);
}

void inaccessibleExit() {
    const NavigationEnvironment env(
        scenarioFrom({"########", "#....#S#", "########"}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, 60));
    const EnergyHelp help = helpFor(env);
    assert(!help.route_found && !help.verdict);
    const auto lines = format_energy_help(help);
    assert(lines[5] == "Recorrido: Destino inaccesible");
    assert(lines.size() == 7);  // sin gasto total ni saldo
    assert(help.rows[0].cost == "1" && help.rows[1].cost == "2");
}

// Mapa del Proyecto 1 (sin interruptores): solo la salida, habilitada.
void withoutObjectives() {
    Grid<Cell> grid(1, 4);
    grid.at({0, 3}) = Exit{};
    const NavigationEnvironment env(grid, {0, 0});
    const EnergyHelp help = helpFor(env);
    assert(help.rows.size() == 1);
    assert(help.rows[0].status == "Habilitada" && help.rows[0].cost == "3");
}

}  // namespace

int main() {
    statementTable();
    energyVerdicts();
    helpDoesNotPlay();
    updatesAfterMovingAndActivating();
    exitEnabled();
    inaccessibleExit();
    withoutObjectives();
    return 0;
}
