#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <variant>
#include <vector>

#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

// Tablero 4x8: borde de muros; interior libre en las filas 1-2 y columnas 1-6.
//   ########
//   #......#     fila 1 = pasillo del jugador
//   #......#     fila 2 = espacio donde se aparta el vigilante
//   ########
Scenario tablero(Position inicio, Position a, Position b, Position salida, Position vigilante) {
    Grid<Cell> grid(4, 8);
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 8; ++c) {
            const bool borde = r == 0 || r == 3 || c == 0 || c == 7;
            grid.at({r, c}) = borde ? Cell{Wall{}} : Cell{Empty{}};
        }
    }
    grid.at(salida) = Exit{};
    Scenario escenario{std::move(grid)};
    escenario.start = inicio;
    escenario.switch_a = a;
    escenario.switch_b = b;
    escenario.guard_start = vigilante;
    escenario.rules = rulesFor(Difficulty::standard);
    return escenario;
}

// A y B sobre el pasillo; el vigilante apartado en la fila 2.
Scenario pasillo() { return tablero({1, 3}, {1, 1}, {1, 5}, {1, 6}, {2, 4}); }
// A en el pasillo y B fuera del camino a la salida.
Scenario conBFueraDelCamino() { return tablero({1, 3}, {1, 1}, {2, 5}, {1, 6}, {2, 2}); }
// Recorrido corto para probar el final: A(1,2), B(1,3), salida (1,5), vigilante pegado a la salida.
Scenario recorridoCorto() { return tablero({1, 1}, {1, 2}, {1, 3}, {1, 5}, {1, 6}); }

// Un turno completo como lo hará GameApplication.
StepResult turno(NavigationEnvironment& entorno, Action accion, Action vigilante = Action::wait) {
    const auto jugador = entorno.step_player(accion);
    if (!jugador.finished && entorno.guard_moves_this_turn()) {
        (void)entorno.step_guard(vigilante);
    }
    return entorno.finish_turn();
}

template<typename Excepcion, typename Callable>
bool lanza(Callable llamada) {
    try {
        llamada();
    } catch (const Excepcion&) {
        return true;
    }
    return false;
}

template<typename Event>
bool contiene(const std::vector<NavigationEvent>& eventos) {
    return std::any_of(eventos.begin(), eventos.end(),
                       [](const NavigationEvent& e) { return std::holds_alternative<Event>(e); });
}

// ------------------------------------------------------------- Orden del turno
void ordenObligatorioDelTurno() {
    NavigationEnvironment e(pasillo());
    assert(lanza<std::logic_error>([&] { (void)e.step_guard(Action::wait); }));   // antes de step_player
    assert(lanza<std::logic_error>([&] { (void)e.finish_turn(); }));               // antes de step_player

    (void)e.step_player(Action::wait);                                             // turno 1 (impar)
    assert(lanza<std::logic_error>([&] { (void)e.step_guard(Action::wait); }));   // impar: no se mueve
    assert(lanza<std::logic_error>([&] { (void)e.step_player(Action::wait); }));  // turno sin cerrar
    (void)e.finish_turn();

    (void)e.step_player(Action::wait);                                             // turno 2 (par)
    assert(lanza<std::logic_error>([&] { (void)e.finish_turn(); }));               // falta el vigilante
    assert(lanza<std::invalid_argument>([&] { (void)e.step_guard(Action::down); }));  // muro
    (void)e.step_guard(Action::wait);
    assert(lanza<std::logic_error>([&] { (void)e.step_guard(Action::wait); }));   // dos veces
    (void)e.finish_turn();
    assert(e.turn() == 2);
}

void elVigilanteSoloSeMueveEnTurnosPares() {
    NavigationEnvironment e(pasillo());
    assert(!e.guard_moves_this_turn());                     // turno 0
    (void)e.step_player(Action::wait);
    assert(!e.guard_moves_this_turn());                     // turno 1
    (void)e.finish_turn();

    const auto jugador = e.step_player(Action::wait);       // turno 2
    assert(e.guard_moves_this_turn());
    const auto vigilante = e.step_guard(Action::up);
    assert((e.guard_position() == Position{1, 4}));
    assert(contiene<GuardMovedEvent>(vigilante.events));
    assert(vigilante.observation.energy == jugador.observation.energy);  // no gasta energia
    const auto cierre = e.finish_turn();
    assert(!cierre.finished);
}

void movimientosLegalesDelVigilante() {
    NavigationEnvironment e(pasillo());                      // vigilante en (2,4)
    const auto legales = e.guard_legal_actions();
    const auto esta = [&](Action a) { return std::find(legales.begin(), legales.end(), a) != legales.end(); };
    assert(esta(Action::up) && esta(Action::left) && esta(Action::right) && esta(Action::wait));
    assert(!esta(Action::down));                             // muro

    Grid<Cell> sola(1, 3);
    sola.at({0, 2}) = Exit{};
    NavigationEnvironment heredado(sola, {0, 0});
    assert(heredado.guard_legal_actions().empty());          // sin vigilante
    assert(!heredado.guard_moves_this_turn());
}

// ----------------------------------------------------------- Interruptores y escape
void escapeConAyBenAmbosOrdenes() {
    {   // A y luego B
        NavigationEnvironment e(pasillo());
        (void)turno(e, Action::left);
        (void)turno(e, Action::left);
        assert(e.is_switch_active(SwitchId::a) && !e.is_switch_active(SwitchId::b));
        for (int i = 0; i < 4; ++i) (void)turno(e, Action::right);
        assert(e.is_switch_active(SwitchId::b));
        assert(!e.isFinished());
        const auto fin = turno(e, Action::right);
        assert(fin.finished && fin.reason == EndReason::goalReached);
        assert(contiene<GoalReachedEvent>(fin.events));
    }
    {   // B y luego A
        NavigationEnvironment e(pasillo());
        (void)turno(e, Action::right);
        (void)turno(e, Action::right);
        assert(e.is_switch_active(SwitchId::b) && !e.is_switch_active(SwitchId::a));
        for (int i = 0; i < 4; ++i) (void)turno(e, Action::left);
        assert(e.is_switch_active(SwitchId::a));
        for (int i = 0; i < 4; ++i) (void)turno(e, Action::right);
        assert(!e.isFinished());
        const auto fin = turno(e, Action::right);
        assert(fin.finished && fin.reason == EndReason::goalReached);
    }
}

void pisarLaSalidaSinInterruptoresNoGana() {
    {   // sin ninguno
        NavigationEnvironment e(conBFueraDelCamino());
        for (int i = 0; i < 3; ++i) (void)turno(e, Action::right);
        assert((e.player_position() == Position{1, 6}));
        assert(!e.isFinished() && e.endReason() == EndReason::none);
    }
    {   // solo A: pisa la salida, no gana; después activa B y entonces sí
        NavigationEnvironment e(conBFueraDelCamino());
        for (int i = 0; i < 2; ++i) (void)turno(e, Action::left);
        for (int i = 0; i < 5; ++i) (void)turno(e, Action::right);
        assert((e.player_position() == Position{1, 6}));
        assert(e.is_switch_active(SwitchId::a) && !e.is_switch_active(SwitchId::b));
        assert(!e.isFinished());

        (void)turno(e, Action::down);
        (void)turno(e, Action::left);                       // (2,5) = B
        assert(e.is_switch_active(SwitchId::b));
        (void)turno(e, Action::right);
        const auto fin = turno(e, Action::up);              // vuelve a la salida
        assert(fin.finished && fin.reason == EndReason::goalReached);
    }
}

void elInterruptorSeAnunciaUnaSolaVez() {
    NavigationEnvironment e(pasillo());
    (void)turno(e, Action::left);
    const auto entra = turno(e, Action::left);               // (1,1) = A
    assert(contiene<SwitchActivatedEvent>(entra.events));
    (void)turno(e, Action::right);
    const auto vuelve = turno(e, Action::left);              // otra vez sobre A
    assert(!contiene<SwitchActivatedEvent>(vuelve.events));
}

// ------------------------------------------------------------------- Captura
void capturaInicialAlPisarAlVigilante() {
    NavigationEnvironment e(tablero({1, 3}, {1, 1}, {2, 5}, {1, 6}, {1, 4}));
    const auto jugador = e.step_player(Action::right);       // entra a (1,4)
    assert(jugador.finished && jugador.reason == EndReason::captured);
    assert(contiene<CapturedEvent>(jugador.events));
    assert(lanza<std::logic_error>([&] { (void)e.step_guard(Action::wait); }));
    const auto cierre = e.finish_turn();                     // idempotente
    assert(cierre.finished && cierre.reason == EndReason::captured && cierre.events.empty());
    assert(e.isFinished() && e.endReason() == EndReason::captured);
    assert(lanza<std::logic_error>([&] { (void)e.step_player(Action::wait); }));
}

void capturaPorMovimientoDelVigilante() {
    NavigationEnvironment e(tablero({1, 3}, {1, 1}, {2, 5}, {1, 6}, {1, 4}));
    (void)turno(e, Action::wait);
    (void)e.step_player(Action::wait);                        // turno 2
    (void)e.step_guard(Action::left);                         // cae sobre el jugador
    const auto cierre = e.finish_turn();
    assert(cierre.finished && cierre.reason == EndReason::captured);
    assert(contiene<CapturedEvent>(cierre.events));
}

void laCapturaTienePrioridadSobreElEscape() {
    NavigationEnvironment e(recorridoCorto());                // salida (1,5), vigilante (1,6)
    (void)turno(e, Action::right);                            // A
    (void)turno(e, Action::right);                            // B
    (void)turno(e, Action::right);
    assert(e.is_switch_active(SwitchId::a) && e.is_switch_active(SwitchId::b));
    const auto fin = turno(e, Action::right, Action::left);   // turno 4: ambos entran a la salida
    assert(fin.finished && fin.reason == EndReason::captured);
    assert(!contiene<GoalReachedEvent>(fin.events));
}

void elInterruptorNoSeActivaSiTeCapturan() {                  // decision 6 del contrato
    NavigationEnvironment e(tablero({1, 1}, {1, 3}, {2, 5}, {1, 6}, {1, 4}));
    (void)turno(e, Action::right);                            // (1,2)
    const auto fin = turno(e, Action::right, Action::left);   // jugador y vigilante a (1,3) = A
    assert(fin.finished && fin.reason == EndReason::captured);
    assert(!e.is_switch_active(SwitchId::a));
    assert(!contiene<SwitchActivatedEvent>(fin.events));
}

// ------------------------------------------------------- Energia y limite de turnos
void energiaCeroEnLaSalidaNoGana() {
    Scenario sin_margen = recorridoCorto();
    sin_margen.rules.initialEnergy = 4;                       // llega a la salida con 0
    sin_margen.rules.maximumEnergy = 4;
    NavigationEnvironment e(sin_margen);
    for (int i = 0; i < 3; ++i) (void)turno(e, Action::right);
    const auto fin = turno(e, Action::right);
    assert((e.player_position() == Position{1, 5}));
    assert(e.energy() == 0);
    assert(fin.finished && fin.reason == EndReason::noEnergy);

    Scenario con_margen = recorridoCorto();
    con_margen.rules.initialEnergy = 5;
    con_margen.rules.maximumEnergy = 5;
    NavigationEnvironment g(con_margen);
    for (int i = 0; i < 3; ++i) (void)turno(g, Action::right);
    const auto gana = turno(g, Action::right);
    assert(g.energy() == 1);
    assert(gana.finished && gana.reason == EndReason::goalReached);
}

void elUltimoTurnoPermitidoCuenta() {
    Scenario justo = recorridoCorto();
    justo.rules.turnLimit = 4;                                // la salida se pisa en el turno 4
    NavigationEnvironment e(justo);
    for (int i = 0; i < 3; ++i) (void)turno(e, Action::right);
    const auto fin = turno(e, Action::right);
    assert(fin.finished && fin.reason == EndReason::goalReached);   // el escape va antes del limite

    Scenario corto = recorridoCorto();
    corto.rules.turnLimit = 3;
    NavigationEnvironment g(corto);
    for (int i = 0; i < 2; ++i) (void)turno(g, Action::right);
    const auto limite = turno(g, Action::right);
    assert(limite.finished && limite.reason == EndReason::turnLimit);
}

// ---------------------------------------------------- Compatibilidad y reinicio
void stepHeredadoConVigilanteQueEspera() {
    NavigationEnvironment e(pasillo());
    const Position vigilante = e.guard_position();
    for (int i = 0; i < 4; ++i) (void)e.step(Action::wait);
    assert(e.turn() == 4);
    assert(e.guard_position() == vigilante);
}

void resetACualquierMomentoDelTurno() {
    NavigationEnvironment e(pasillo());
    (void)e.step_player(Action::left);                        // turno a medias
    e.reset(1);
    assert(e.turn() == 0 && (e.player_position() == Position{1, 3}));
    (void)e.step_player(Action::left);                        // vuelve a aceptar un turno nuevo
    (void)e.finish_turn();
    assert(e.turn() == 1);
}

void mismaSecuenciaMismaPartida() {
    const auto jugar = [] {
        NavigationEnvironment e(pasillo());
        const Action plan[] = {Action::left, Action::left, Action::right, Action::right,
                               Action::right, Action::right, Action::right};
        StepResult fin{};
        for (Action a : plan) {
            const auto jugador = e.step_player(a);
            if (!jugador.finished && e.guard_moves_this_turn()) {
                // El vigilante elige siempre su primera accion legal (determinista).
                (void)e.step_guard(e.guard_legal_actions().front());
            }
            fin = e.finish_turn();
        }
        return std::make_tuple(e.player_position().row, e.player_position().column, e.energy(),
                               fin.finished, static_cast<int>(fin.reason));
    };
    assert(jugar() == jugar());
}

}  // namespace

int main() {
    ordenObligatorioDelTurno();
    elVigilanteSoloSeMueveEnTurnosPares();
    movimientosLegalesDelVigilante();
    escapeConAyBenAmbosOrdenes();
    pisarLaSalidaSinInterruptoresNoGana();
    elInterruptorSeAnunciaUnaSolaVez();
    capturaInicialAlPisarAlVigilante();
    capturaPorMovimientoDelVigilante();
    laCapturaTienePrioridadSobreElEscape();
    elInterruptorNoSeActivaSiTeCapturan();
    energiaCeroEnLaSalidaNoGana();
    elUltimoTurnoPermitidoCuenta();
    stepHeredadoConVigilanteQueEspera();
    resetACualquierMomentoDelTurno();
    mismaSecuenciaMismaPartida();
    return 0;
}