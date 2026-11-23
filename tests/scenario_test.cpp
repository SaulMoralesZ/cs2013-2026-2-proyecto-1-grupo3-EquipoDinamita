#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

Scenario escenarioValido() {
    Grid<Cell> grid(4, 5);
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 5; ++c) {
            const bool borde = r == 0 || r == 3 || c == 0 || c == 4;
            grid.at({r, c}) = borde ? Cell{Wall{}} : Cell{Empty{}};
        }
    }
    grid.at({2, 3}) = Exit{};
    Scenario escenario{std::move(grid)};
    escenario.start = {1, 1};
    escenario.switch_a = {1, 2};
    escenario.switch_b = {1, 3};
    escenario.guard_start = {2, 1};
    escenario.rules = rulesFor(Difficulty::standard);
    return escenario;
}

template<typename Mutation>
void debeRechazar(Mutation mutar) {
    Scenario escenario = escenarioValido();
    mutar(escenario);
    bool lanzo = false;
    try {
        NavigationEnvironment entorno(escenario);
    } catch (const std::invalid_argument&) {
        lanzo = true;
    }
    assert(lanzo);
}

void escenarioValidoConstruyeYLee() {
    const Scenario escenario = escenarioValido();
    NavigationEnvironment entorno(escenario);

    assert(entorno.has_objectives());
    assert((entorno.player_position() == Position{1, 1}));
    assert((entorno.guard_position() == Position{2, 1}));
    assert((entorno.switch_position(SwitchId::a) == Position{1, 2}));
    assert((entorno.switch_position(SwitchId::b) == Position{1, 3}));
    assert(!entorno.is_switch_active(SwitchId::a));
    assert(!entorno.is_switch_active(SwitchId::b));
    assert((entorno.goal() == Position{2, 3}));
    assert(entorno.energy() == escenario.rules.initialEnergy);
    assert(entorno.turn() == 0);
    assert(entorno.grid().rows() == 4 && entorno.grid().columns() == 5);
}

void rechazaSalidasInvalidas() {
    debeRechazar([](Scenario& s) { s.grid.at({2, 3}) = Empty{}; });// sin salida
    debeRechazar([](Scenario& s) { s.grid.at({2, 2}) = Exit{}; });// dos salidas
}

void rechazaPosicionesFueraDelTablero() {
    debeRechazar([](Scenario& s) { s.start = {9, 9}; });
    debeRechazar([](Scenario& s) { s.switch_a = {4, 0}; });
    debeRechazar([](Scenario& s) { s.switch_b = {0, 5}; });
    debeRechazar([](Scenario& s) { s.guard_start = {100, 100}; });
}

void rechazaCeldasNoValidas() {
    debeRechazar([](Scenario& s) { s.grid.at(s.start) = Wall{}; });
    debeRechazar([](Scenario& s) { s.grid.at(s.switch_a) = Wall{}; });
    debeRechazar([](Scenario& s) { s.switch_b = {2, 3}; });// sobre la salida
    debeRechazar([](Scenario& s) { s.grid.at(s.guard_start) = Exit{}; s.grid.at({2, 3}) = Empty{}; });
    debeRechazar([](Scenario& s) { s.grid.at(s.switch_a) = Battery{}; });
    debeRechazar([](Scenario& s) { s.grid.at(s.guard_start) = Trap{}; });
}

void rechazaPosicionesRepetidas() {
    debeRechazar([](Scenario& s) { s.switch_a = s.start; });
    debeRechazar([](Scenario& s) { s.switch_b = s.switch_a; });
    debeRechazar([](Scenario& s) { s.guard_start = s.switch_b; });
    debeRechazar([](Scenario& s) { s.guard_start = s.start; });
}

void rechazaReglasInvalidas() {
    debeRechazar([](Scenario& s) { s.rules.initialEnergy = 0; });
    debeRechazar([](Scenario& s) { s.rules.maximumEnergy = s.rules.initialEnergy - 1; });
    debeRechazar([](Scenario& s) { s.rules.turnLimit = 0; });
}

void terrenoAsperoEsValido() {
    Scenario escenario = escenarioValido();
    escenario.grid.at(escenario.switch_a) = RoughTerrain{};
    escenario.grid.at(escenario.guard_start) = RoughTerrain{};
    NavigationEnvironment entorno(escenario);
    assert(entorno.has_objectives());
}

void resetRestauraInterruptoresYVigilante() {
    NavigationEnvironment entorno(escenarioValido());
    (void)entorno.step(Action::right);
    assert(entorno.turn() == 1);

    entorno.reset(3);
    assert(entorno.turn() == 0);
    assert((entorno.player_position() == Position{1, 1}));
    assert((entorno.guard_position() == Position{2, 1}));
    assert(!entorno.is_switch_active(SwitchId::a));
    assert(!entorno.is_switch_active(SwitchId::b));
    assert(entorno.energy() == entorno.rules().initialEnergy);
}

void constructorHeredadoNoTieneObjetivos() {
    Grid<Cell> grid(1, 3);
    grid.at({0, 2}) = Exit{};
    NavigationEnvironment entorno(grid, {0, 0});
    assert(!entorno.has_objectives());

    bool lanzoVigilante = false;
    bool lanzoInterruptor = false;
    try { (void)entorno.guard_position(); } catch (const std::logic_error&) { lanzoVigilante = true; }
    try { (void)entorno.is_switch_active(SwitchId::a); } catch (const std::logic_error&) { lanzoInterruptor = true; }
    assert(lanzoVigilante && lanzoInterruptor);
}

void costoDeEntradaCompartido() {
    const GameRules reglas = rulesFor(Difficulty::standard);
    assert(entry_cost(Cell{Empty{}}, reglas) == reglas.baseCost);
    assert(entry_cost(Cell{RoughTerrain{}}, reglas) == reglas.roughCost);
    assert(entry_cost(Cell{Battery{}}, reglas) == reglas.baseCost);
    assert(entry_cost(Cell{Trap{}}, reglas) == reglas.baseCost);
    assert(entry_cost(Cell{Exit{}}, reglas) == reglas.baseCost);
    assert(reglas.roughCost != reglas.baseCost);
}

void ordenDePosiciones() {
    assert((Position{1, 5} < Position{2, 0}));
    assert((Position{2, 1} < Position{2, 3}));
    assert((Position{2, 3} == Position{2, 3}));
    std::set<Position> conjunto{{2, 3}, {1, 5}, {2, 3}, {0, 0}};
    assert(conjunto.size() == 3);
    assert((*conjunto.begin() == Position{0, 0}));
}

void parseScenarioSigueDandoTableroEInicio() {
    const std::string filas[] = {"#####", "#@..#", "#..S#", "#####"};
    const auto diseno = parseScenario(filas);
    assert(diseno.grid.rows() == 4 && diseno.grid.columns() == 5);
    assert((diseno.start == Position{1, 1}));
}

void readLinesIgnoraElRetornoDeCarro() {
    const auto ruta = std::filesystem::temp_directory_path() / "circuit_escape_crlf_test.txt";
    {
        std::ofstream archivo(ruta, std::ios::binary);
        archivo << "#####\r\n#@.S#\r\n#####\r\n";
    }
    const auto lineas = readLines(ruta);
    std::filesystem::remove(ruta);

    assert(lineas.size() == 3);
    for (const auto& linea : lineas) assert(linea.size() == 5 && linea.back() != '\r');
    const auto diseno = parseScenario(lineas);
    assert(diseno.grid.rows() == 3 && diseno.grid.columns() == 5);
}

}

int main() {
    escenarioValidoConstruyeYLee();
    rechazaSalidasInvalidas();
    rechazaPosicionesFueraDelTablero();
    rechazaCeldasNoValidas();
    rechazaPosicionesRepetidas();
    rechazaReglasInvalidas();
    terrenoAsperoEsValido();
    resetRestauraInterruptoresYVigilante();
    constructorHeredadoNoTieneObjetivos();
    costoDeEntradaCompartido();
    ordenDePosiciones();
    parseScenarioSigueDandoTableroEInicio();
    readLinesIgnoraElRetornoDeCarro();
    return 0;
}