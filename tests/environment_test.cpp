#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <variant>

#include "circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

Grid<Cell, 3, 4> tableroBase() {
    Grid<Cell, 3, 4> grid;
    for (std::size_t column = 0; column < 4; ++column) {
        grid.at({0, column}) = Wall{};
        grid.at({2, column}) = Wall{};
    }
    grid.at({1, 0}) = Wall{};
    grid.at({1, 3}) = Exit{};
    return grid;
}

void precondicionesDelConstructor() {
    Grid<Cell, 3, 4> sinSalida = tableroBase();
    sinSalida.at({1, 3}) = Empty{};
    bool lanzoSinSalida = false;
    try {
        NavigationEnvironment<3, 4> entorno(sinSalida, {1, 1});
    } catch (const std::invalid_argument&) {
        lanzoSinSalida = true;
    }
    assert(lanzoSinSalida);

    Grid<Cell, 3, 4> dosSalidas = tableroBase();
    dosSalidas.at({1, 2}) = Exit{};
    bool lanzoDosSalidas = false;
    try {
        NavigationEnvironment<3, 4> entorno(dosSalidas, {1, 1});
    } catch (const std::invalid_argument&) {
        lanzoDosSalidas = true;
    }
    assert(lanzoDosSalidas);

    bool lanzoInicioBloqueado = false;
    try {
        NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 0});
    } catch (const std::invalid_argument&) {
        lanzoInicioBloqueado = true;
    }
    assert(lanzoInicioBloqueado);

    bool lanzoFueraDelTablero = false;
    try {
        NavigationEnvironment<3, 4> entorno(tableroBase(), {5, 5});
    } catch (const std::invalid_argument&) {
        lanzoFueraDelTablero = true;
    }
    assert(lanzoFueraDelTablero);
}

void estadoInicial() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1}, reglas);
    const auto estado = entorno.state();

    assert((estado.agent == Position{1, 1}));
    assert((estado.goal == Position{1, 3}));
    assert(estado.energy == reglas.initialEnergy);
    assert(estado.maximumEnergy == reglas.maximumEnergy);
    assert(estado.score == 0);
    assert(estado.collectedResources == 0);
    assert(estado.turn == 0);
    assert(!entorno.isFinished());
}

void movimientoLibreYCosto() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1}, reglas);

    const auto resultado = entorno.step(Action::right);
    assert((resultado.observation.agent == Position{1, 2}));
    assert(resultado.observation.energy == reglas.initialEnergy - reglas.baseCost);
    assert(resultado.observation.turn == 1);
    assert(!resultado.finished);

    bool hayMovimiento = false;
    bool hayCambioDeEnergia = false;
    for (const auto& evento : resultado.events) {
        if (std::holds_alternative<MovedEvent>(evento)) hayMovimiento = true;
        if (std::holds_alternative<EnergyChangedEvent>(evento)) hayCambioDeEnergia = true;
    }
    assert(hayMovimiento);
    assert(hayCambioDeEnergia);
}

void costoDeEsperaYDeTerrenoElevado() {
    const auto reglas = rulesFor(Difficulty::standard);

    NavigationEnvironment<3, 4> esperando(tableroBase(), {1, 1}, reglas);
    const auto trasEsperar = esperando.step(Action::wait);
    assert((trasEsperar.observation.agent == Position{1, 1}));
    assert(trasEsperar.observation.energy == reglas.initialEnergy - reglas.waitCost);
    assert(trasEsperar.observation.turn == 1);

    Grid<Cell, 3, 4> conTerrenoElevado = tableroBase();
    conTerrenoElevado.at({1, 2}) = RoughTerrain{};
    NavigationEnvironment<3, 4> elevado(conTerrenoElevado, {1, 1}, reglas);
    const auto trasElevado = elevado.step(Action::right);
    assert(trasElevado.observation.energy == reglas.initialEnergy - reglas.roughCost);
    assert(reglas.roughCost != reglas.baseCost);
}

void llegadaALaSalida() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    (void)entorno.step(Action::right);
    const auto resultado = entorno.step(Action::right);

    assert((resultado.observation.agent == Position{1, 3}));
    assert(resultado.finished);
    assert(resultado.reason == EndReason::goalReached);
    assert(entorno.isFinished());

    bool hayLlegada = false;
    for (const auto& evento : resultado.events) {
        if (std::holds_alternative<GoalReachedEvent>(evento)) hayLlegada = true;
    }
    assert(hayLlegada);
}

void terminoPorLimiteDeTurnos() {
    GameRules reglas = rulesFor(Difficulty::standard);
    reglas.turnLimit = 2;
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1}, reglas);

    const auto primero = entorno.step(Action::wait);
    assert(!primero.finished);

    const auto segundo = entorno.step(Action::wait);
    assert(segundo.finished);
    assert(segundo.reason == EndReason::turnLimit);
}

void stepDespuesDelTermino() {
    GameRules reglas = rulesFor(Difficulty::standard);
    reglas.turnLimit = 1;
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1}, reglas);
    (void)entorno.step(Action::wait);
    assert(entorno.isFinished());

    bool lanzo = false;
    try {
        (void)entorno.step(Action::wait);
    } catch (const std::logic_error&) {
        lanzo = true;
    }
    assert(lanzo);
}

void resetRestauraLaPartida() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    (void)entorno.step(Action::right);
    (void)entorno.step(Action::right);
    assert(entorno.isFinished());

    entorno.reset(7);
    const auto estado = entorno.state();
    assert((estado.agent == Position{1, 1}));
    assert(estado.turn == 0);
    assert(estado.energy == entorno.rules().initialEnergy);
    assert(!entorno.isFinished());
    assert(entorno.endReason() == EndReason::none);
    assert(entorno.seed() == 7);
}

void perfilesDeDificultad() {
    const auto facil = rulesFor(Difficulty::easy);
    assert(facil.initialEnergy == 80 && facil.maximumEnergy == 80);
    assert(facil.turnLimit == 240);
    assert(facil.baseCost == 1 && facil.roughCost == 2);
    assert(facil.waitCost == 1 && facil.invalidCost == 1);
    assert(facil.resourcePoints == 15 && facil.batteryRecharge == 5);
    assert(facil.trapEnergyPenalty == 1 && facil.trapScorePenalty == 0);

    const auto normal = rulesFor(Difficulty::standard);
    assert(normal.initialEnergy == 60 && normal.maximumEnergy == 60);
    assert(normal.turnLimit == 180);
    assert(normal.baseCost == 1 && normal.roughCost == 2);
    assert(normal.waitCost == 1 && normal.invalidCost == 1);
    assert(normal.resourcePoints == 10 && normal.batteryRecharge == 3);
    assert(normal.trapEnergyPenalty == 2 && normal.trapScorePenalty == 1);

    const auto duro = rulesFor(Difficulty::hard);
    assert(duro.initialEnergy == 40 && duro.maximumEnergy == 40);
    assert(duro.turnLimit == 140);
    assert(duro.baseCost == 1 && duro.roughCost == 3);
    assert(duro.waitCost == 1 && duro.invalidCost == 1);
    assert(duro.resourcePoints == 8 && duro.batteryRecharge == 2);
    assert(duro.trapEnergyPenalty == 3 && duro.trapScorePenalty == 2);

    assert(parseDifficulty("standard").value() == Difficulty::standard);
    assert(!parseDifficulty("imposible").has_value());
    assert(toString(Difficulty::hard) == "hard");
}

}

int main() {
    precondicionesDelConstructor();
    estadoInicial();
    movimientoLibreYCosto();
    costoDeEsperaYDeTerrenoElevado();
    llegadaALaSalida();
    terminoPorLimiteDeTurnos();
    stepDespuesDelTermino();
    resetRestauraLaPartida();
    perfilesDeDificultad();
    return 0;
}
