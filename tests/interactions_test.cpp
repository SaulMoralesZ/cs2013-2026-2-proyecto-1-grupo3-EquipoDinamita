#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <variant>
#include <vector>

#include "circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

Grid<Cell, 3, 4> tableroCon(Cell enColumnaDos) {
    Grid<Cell, 3, 4> grid;
    for (std::size_t column = 0; column < 4; ++column) {
        grid.at({0, column}) = Wall{};
        grid.at({2, column}) = Wall{};
    }
    grid.at({1, 0}) = Wall{};
    grid.at({1, 2}) = enColumnaDos;
    grid.at({1, 3}) = Exit{};
    return grid;
}

template<typename Event>
bool contiene(const std::vector<NavigationEvent>& events) {
    for (const auto& event : events) {
        if (std::holds_alternative<Event>(event)) return true;
    }
    return false;
}

void movimientoContraUnMuro() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroCon(Empty{}), {1, 1}, reglas);

    const auto resultado = entorno.step(Action::up);  // (0,1) es muro
    assert((resultado.observation.agent == Position{1, 1}));
    assert(resultado.observation.energy == reglas.initialEnergy - reglas.invalidCost);
    assert(resultado.observation.turn == 1);
    assert(contiene<MovementRejectedEvent>(resultado.events));
    assert(!contiene<MovedEvent>(resultado.events));
}

void movimientoFueraDelTablero() {
    const auto reglas = rulesFor(Difficulty::standard);

    Grid<Cell, 1, 3> grid;
    grid.at({0, 2}) = Exit{};
    NavigationEnvironment<1, 3> entorno(grid, {0, 0}, reglas);

    const auto resultado = entorno.step(Action::up);
    assert((resultado.observation.agent == Position{0, 0}));
    assert(resultado.observation.energy == reglas.initialEnergy - reglas.invalidCost);
    assert(contiene<MovementRejectedEvent>(resultado.events));
}

void recoleccionUnicaDelRecurso() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroCon(ResourceCell<int>{}), {1, 1}, reglas);

    const auto primera = entorno.step(Action::right);
    assert(primera.observation.score == reglas.resourcePoints);
    assert(primera.observation.collectedResources == 1);
    assert(contiene<ResourceCollectedEvent>(primera.events));

    (void)entorno.step(Action::left);
    const auto segunda = entorno.step(Action::right);
    assert(segunda.observation.score == reglas.resourcePoints);
    assert(segunda.observation.collectedResources == 1);
    assert(!contiene<ResourceCollectedEvent>(segunda.events));
}

void bateriaRecargaTrasPagarElCostoDeEntrada() {
    GameRules reglas = rulesFor(Difficulty::standard);
    reglas.initialEnergy = 1;
    reglas.maximumEnergy = 10;
    NavigationEnvironment<3, 4> entorno(tableroCon(Battery{}), {1, 1}, reglas);

    const auto resultado = entorno.step(Action::right);
    assert(resultado.observation.energy == reglas.batteryRecharge);
    assert(!resultado.finished);
    assert(resultado.reason == EndReason::none);
}

void bateriaNoSuperaLaEnergiaMaxima() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroCon(Battery{}), {1, 1}, reglas);

    const auto resultado = entorno.step(Action::right);
    assert(resultado.observation.energy == reglas.maximumEnergy);
}

void bateriaEsConsumible() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroCon(Battery{}), {1, 1}, reglas);

    (void)entorno.step(Action::right);
    (void)entorno.step(Action::left);
    const auto energiaAntes = entorno.state().energy;
    const auto segunda = entorno.step(Action::right);

    assert(segunda.observation.energy == energiaAntes - reglas.baseCost);
}

void trampaSeActivaCadaVez() {
    const auto reglas = rulesFor(Difficulty::standard);
    NavigationEnvironment<3, 4> entorno(tableroCon(Trap{}), {1, 1}, reglas);

    const auto primera = entorno.step(Action::right);
    assert(primera.observation.energy ==
           reglas.initialEnergy - reglas.baseCost - reglas.trapEnergyPenalty);
    assert(primera.observation.score == -reglas.trapScorePenalty);
    assert(contiene<TrapTriggeredEvent>(primera.events));

    (void)entorno.step(Action::left);
    const auto energiaAntes = entorno.state().energy;
    const auto segunda = entorno.step(Action::right);

    assert(segunda.observation.energy ==
           energiaAntes - reglas.baseCost - reglas.trapEnergyPenalty);
    assert(segunda.observation.score == -2 * reglas.trapScorePenalty);
    assert(contiene<TrapTriggeredEvent>(segunda.events));
}

void procesamientoDeCadaTipoDeEvento() {
    const std::vector<NavigationEvent> eventos{
        MovedEvent{{0, 0}, {0, 1}, 1},
        MovementRejectedEvent{{0, 1}, Action::up},
        ResourceCollectedEvent{{0, 1}, 10},
        EnergyChangedEvent{10, 8},
        TrapTriggeredEvent{{0, 1}},
        GoalReachedEvent{{0, 2}},
    };

    std::size_t visitados = 0;
    for (const auto& evento : eventos) {
        std::visit(Overloaded{
                       [&](const MovedEvent&) { ++visitados; },
                       [&](const MovementRejectedEvent&) { ++visitados; },
                       [&](const ResourceCollectedEvent&) { ++visitados; },
                       [&](const EnergyChangedEvent&) { ++visitados; },
                       [&](const TrapTriggeredEvent&) { ++visitados; },
                       [&](const GoalReachedEvent&) { ++visitados; },
                   },
                   evento);
        assert(!describe(evento).empty());
    }
    assert(visitados == eventos.size());
}

}

int main() {
    movimientoContraUnMuro();
    movimientoFueraDelTablero();
    recoleccionUnicaDelRecurso();
    bateriaRecargaTrasPagarElCostoDeEntrada();
    bateriaNoSuperaLaEnergiaMaxima();
    bateriaEsConsumible();
    trampaSeActivaCadaVez();
    procesamientoDeCadaTipoDeEvento();
    return 0;
}
