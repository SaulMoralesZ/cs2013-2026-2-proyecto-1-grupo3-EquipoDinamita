#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../include/circuit_escape/controllers.hpp"

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

Grid<Cell, 3, 3> tableroAbierto() {
    Grid<Cell, 3, 3> grid;
    grid.at({2, 2}) = Exit{};
    return grid;
}

bool contieneAccion(const std::vector<Action>& acciones, Action buscada) {
    for (Action accion : acciones) {
        if (accion == buscada) return true;
    }
    return false;
}

void accionesDisponiblesJuntoAUnMuro() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    const auto acciones = entorno.availableActions();

    assert(acciones.size() == 2);
    assert(contieneAccion(acciones, Action::right));
    assert(contieneAccion(acciones, Action::wait));
    assert(!contieneAccion(acciones, Action::up));
    assert(!contieneAccion(acciones, Action::left));
}

void accionesDisponiblesEnUnaEsquina() {
    Grid<Cell, 1, 3> grid;
    grid.at({0, 2}) = Exit{};
    NavigationEnvironment<1, 3> entorno(grid, {0, 0});
    const auto acciones = entorno.availableActions();

    assert(acciones.size() == 2);
    assert(contieneAccion(acciones, Action::right));
    assert(contieneAccion(acciones, Action::wait));
}

void accionesDisponiblesDespuesDelTermino() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    (void)entorno.step(Action::right);
    (void)entorno.step(Action::right);
    assert(entorno.isFinished());

    assert(entorno.availableActions().empty());
    assert(entorno.state().availableActions.empty());
}

void politicaAleatoriaSoloEligeAccionesLegales() {
    NavigationEnvironment<3, 3> entorno(tableroAbierto(), {0, 0});
    auto controlador = makeController(ControllerKind::random, 123);
    assert(controlador != nullptr);

    for (int paso = 0; paso < 20 && !entorno.isFinished(); ++paso) {
        const auto estado = entorno.state();
        const Action elegida = controlador->selectAction(estado, estado.availableActions);
        assert(contieneAccion(estado.availableActions, elegida));
        (void)entorno.step(elegida);
    }
}

void mismaSemillaMismaSimulacion() {
    const auto correr = [](std::uint32_t semilla) {
        NavigationEnvironment<3, 3> entorno(tableroAbierto(), {0, 0});
        entorno.reset(semilla);
        auto controlador = makeController(ControllerKind::random, semilla);

        std::vector<Action> acciones;
        for (int paso = 0; paso < 30 && !entorno.isFinished(); ++paso) {
            const auto estado = entorno.state();
            const Action elegida =
                controlador->selectAction(estado, estado.availableActions);
            acciones.push_back(elegida);
            (void)entorno.step(elegida);
        }
        return acciones;
    };

    const auto primera = correr(42);
    assert(primera.size() > 1);      // hubo decisiones reales que comparar
    assert(primera == correr(42));   // misma semilla, misma simulacion
}

void politicaHeuristicaAlcanzaLaSalida() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    auto controlador = makeController(ControllerKind::heuristic, 0);

    int pasos = 0;
    while (!entorno.isFinished() && pasos < 20) {
        const auto estado = entorno.state();
        (void)entorno.step(controlador->selectAction(estado, estado.availableActions));
        ++pasos;
    }

    assert(entorno.isFinished());
    assert(entorno.endReason() == EndReason::goalReached);
}

void controladorHumanoRecibeLaDecision() {
    NavigationEnvironment<3, 4> entorno(tableroBase(), {1, 1});
    HumanController humano;
    humano.submit(Action::right);

    const auto estado = entorno.state();
    const Action elegida = humano.selectAction(estado, estado.availableActions);
    assert(elegida == Action::right);

    bool lanzo = false;
    try {
        (void)humano.selectAction(estado, estado.availableActions);
    } catch (const std::logic_error&) {
        lanzo = true;
    }
    assert(lanzo);
}

void mejorAccionConDosContenedores() {
    const std::vector<Action> comoVector{Action::up, Action::right, Action::down};
    const std::list<Action> comoLista{Action::up, Action::right, Action::down};
    const auto prefiereDerecha = [](Action accion) {
        return accion == Action::right ? 1 : 0;
    };

    const auto desdeVector =
        bestAction(comoVector.begin(), comoVector.end(), prefiereDerecha);
    const auto desdeLista =
        bestAction(comoLista.begin(), comoLista.end(), prefiereDerecha);

    assert(desdeVector.has_value() && *desdeVector == Action::right);
    assert(desdeLista.has_value() && *desdeLista == Action::right);

    const std::vector<Action> vacio;
    assert(!bestAction(vacio.begin(), vacio.end(), prefiereDerecha).has_value());
}

void concepts() {
    static_assert(NavigationPolicy<RandomPolicy>);
    static_assert(NavigationPolicy<HeuristicPolicy>);

    struct SinSelectAction {};
    static_assert(!NavigationPolicy<SinSelectAction>);
}

}

int main() {
    accionesDisponiblesJuntoAUnMuro();
    accionesDisponiblesEnUnaEsquina();
    accionesDisponiblesDespuesDelTermino();
    politicaAleatoriaSoloEligeAccionesLegales();
    mismaSemillaMismaSimulacion();
    politicaHeuristicaAlcanzaLaSalida();
    controladorHumanoRecibeLaDecision();
    mejorAccionConDosContenedores();
    concepts();
    return 0;
}
