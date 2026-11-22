#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <variant>

#include "../include/circuit_escape/controllers.hpp"
#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

// Tablero 4x8: borde de muros; interior libre en las filas 1-2 y columnas 1-6.
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

// Registro compartido: la política vive dentro de PolicyController (por valor),
// así que la prueba la observa a través de este objeto.
struct Registro {
    int llamadas{0};
    bool consistente{true};
};

// Política de vigilante de prueba: usa la observación completa (guard, agent,
// map) y los movimientos legales. Es lo que hará la GuardPolicy de Rafael,
// pero con distancia Manhattan en vez de BFS.
class VigilanteVoraz {
public:
    VigilanteVoraz(const NavigationEnvironment& entorno, Registro& registro)
        : entorno_(&entorno), registro_(&registro) {}

    Action selectAction(const Observation& obs, std::span<const Action> legales) {
        ++registro_->llamadas;
        if (obs.map != &entorno_->grid() || !(obs.guard == entorno_->guard_position()) ||
            !(obs.agent == entorno_->player_position())) {
            registro_->consistente = false;
        }
        Action mejor = Action::wait;
        std::size_t mejorDistancia = manhattanDistance(obs.guard, obs.agent);
        for (Action accion : legales) {
            if (accion == Action::wait) continue;
            const auto destino = neighbor(obs.guard, accion);
            if (!destino || !obs.map->contains(*destino)) continue;
            const std::size_t distancia = manhattanDistance(*destino, obs.agent);
            if (distancia < mejorDistancia) {
                mejorDistancia = distancia;
                mejor = accion;
            }
        }
        return mejor;
    }

private:
    const NavigationEnvironment* entorno_;
    Registro* registro_;
};

static_assert(NavigationPolicy<VigilanteVoraz>);

StepResult turno(NavigationEnvironment& entorno, IController& vigilante, Action accion) {
    const auto jugador = entorno.step_player(accion);
    if (!jugador.finished && entorno.guard_moves_this_turn()) {
        const auto obs = entorno.state();
        (void)entorno.step_guard(
            vigilante.selectAction(obs, entorno.guard_legal_actions()));
    }
    return entorno.finish_turn();
}

void laObservacionTraeMapaVigilanteEInterruptores() {
    NavigationEnvironment e(tablero({1, 1}, {1, 2}, {1, 3}, {1, 5}, {2, 6}));
    const Observation obs = e.state();

    assert(obs.map == &e.grid());
    assert(obs.map->rows() == 4 && obs.map->columns() == 8);
    assert(std::holds_alternative<Wall>(obs.map->at({0, 0})));
    assert(std::holds_alternative<Exit>(obs.map->at({1, 5})));
    assert((obs.guard == Position{2, 6}));
    assert((obs.agent == Position{1, 1}));
    assert(!obs.switch_a_active && !obs.switch_b_active);
}

void losInterruptoresYElVigilanteSeReflejanEnLaObservacion() {
    NavigationEnvironment e(tablero({1, 1}, {1, 2}, {1, 3}, {1, 5}, {2, 6}));
    (void)e.step_player(Action::right);
    const auto cierre = e.finish_turn();                       // (1,2) = A
    assert(cierre.observation.switch_a_active && !cierre.observation.switch_b_active);

    (void)e.step_player(Action::right);                        // turno 2 (par)
    const auto movido = e.step_guard(Action::left);            // vigilante a (2,5)
    assert((movido.observation.guard == Position{2, 5}));
    const auto fin = e.finish_turn();                          // (1,3) = B
    assert(fin.observation.switch_a_active && fin.observation.switch_b_active);

    e.reset(0);
    const Observation tras = e.state();
    assert(!tras.switch_a_active && !tras.switch_b_active);
    assert((tras.guard == Position{2, 6}));
    assert(tras.map == &e.grid());                             // reset no cambia la direccion
}

void partidaHeredadaTambienTraeMapa() {
    Grid<Cell> grid(1, 3);
    grid.at({0, 2}) = Exit{};
    NavigationEnvironment e(grid, {0, 0});
    const Observation obs = e.state();
    assert(obs.map == &e.grid());
    assert(!obs.switch_a_active && !obs.switch_b_active);
    assert((obs.guard == Position{}));
}

void unaPoliticaDeVigilanteSeAdaptaConPolicyController() {
    NavigationEnvironment e(tablero({1, 1}, {2, 1}, {1, 2}, {1, 5}, {2, 6}));
    Registro registro;
    std::unique_ptr<IController> vigilante =
        std::make_unique<PolicyController<VigilanteVoraz>>(VigilanteVoraz(e, registro));

    // El jugador se queda quieto; el vigilante lo persigue y lo captura.
    StepResult fin{};
    std::size_t turnos = 0;
    while (!e.isFinished() && turnos < 40) {
        fin = turno(e, *vigilante, Action::wait);
        ++turnos;
    }
    assert(e.isFinished());
    assert(fin.reason == EndReason::captured);
    assert(e.turn() <= 14);                                    // 6 pasos del vigilante, uno cada 2 turnos
    assert(registro.llamadas == static_cast<int>(e.turn() / 2));
    assert(registro.consistente);
}

}  // namespace

int main() {
    laObservacionTraeMapaVigilanteEInterruptores();
    losInterruptoresYElVigilanteSeReflejanEnLaObservacion();
    partidaHeredadaTambienTraeMapa();
    unaPoliticaDeVigilanteSeAdaptaConPolicyController();
    return 0;
}