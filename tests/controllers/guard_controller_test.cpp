#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "circuit_escape/controllers/guard_controller.hpp"
#include "circuit_escape/environment.hpp"
#include "circuit_escape/game_turn.hpp"
#include "circuit_escape/graph/graph.hpp"
#include "circuit_escape/pathfinding/bfs.hpp"

using namespace circuit_escape;

namespace {

// '#' muro, 'S' salida, cualquier otro simbolo libre.
Grid<Cell> board(const std::vector<std::string>& rows) {
    Grid<Cell> grid(rows.size(), rows.front().size());
    for (std::size_t r = 0; r < rows.size(); ++r)
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const char ch = rows[r][c];
            grid.at({r, c}) = ch == '#' ? Cell{Wall{}} : ch == 'S' ? Cell{Exit{}} : Cell{Empty{}};
        }
    return grid;
}

Scenario scenario(const std::vector<std::string>& rows, Position start, Position a, Position b,
                  Position guard) {
    Scenario s{board(rows)};
    s.start = start;
    s.switch_a = a;
    s.switch_b = b;
    s.guard_start = guard;
    s.rules = rulesFor(Difficulty::standard);
    return s;
}

// Anillo alrededor de un bloque de muros: dos caminos entre arriba y abajo.
const std::vector<std::string> kRing = {
    "#######",
    "#.....#",
    "#.###.#",
    "#....S#",
    "#######",
};

Observation seen(Position guard, Position player, const Grid<Cell>& grid) {
    Observation o;
    o.guard = guard;
    o.agent = player;
    o.map = &grid;
    return o;
}

const std::vector<Action> kAllMoves = {Action::up, Action::down, Action::left, Action::right,
                                       Action::wait};

// ----------------------------------------------------------- Decision BFS
void recalculatesTowardCurrentPlayer() {
    const auto grid = board(kRing);
    GuardPolicy policy(std::make_shared<const Graph>(grid));
    // Misma posicion del vigilante; el jugador cambia de lado -> cambia la decision.
    assert(policy.selectAction(seen({1, 3}, {3, 1}, grid), kAllMoves) == Action::left);
    assert(policy.selectAction(seen({1, 3}, {3, 5}, grid), kAllMoves) == Action::right);
    assert(policy.selectAction(seen({1, 3}, {1, 1}, grid), kAllMoves) == Action::left);
    assert(policy.selectAction(seen({1, 1}, {3, 1}, grid), kAllMoves) == Action::down);
}

void respectsWallsAndFixedTieBreak() {
    const auto grid = board(kRing);
    GuardPolicy policy(std::make_shared<const Graph>(grid));
    // (1,3) -> (3,3): abajo es muro; izquierda y derecha empatan en 6 pasos.
    // Orden fijo arriba, abajo, izquierda, derecha => izquierda, siempre.
    for (int i = 0; i < 5; ++i) {
        assert(policy.selectAction(seen({1, 3}, {3, 3}, grid), kAllMoves) == Action::left);
    }
}

void waitsWhenAlreadyThereOrIllegal() {
    const auto grid = board(kRing);
    GuardPolicy policy(std::make_shared<const Graph>(grid));
    assert(policy.selectAction(seen({1, 3}, {1, 3}, grid), kAllMoves) == Action::wait);
    // Si el motor no declara legal el paso BFS, espera.
    const std::vector<Action> onlyWait = {Action::wait};
    assert(policy.selectAction(seen({1, 3}, {3, 1}, grid), onlyWait) == Action::wait);
}

// ------------------------------------------------------ Dentro de la partida
void movesOnlyOnEvenTurnsAlongShortestPath() {
    NavigationEnvironment env(scenario(kRing, {3, 1}, {1, 1}, {3, 4}, {1, 5}));
    const auto guard = make_guard_controller(env.grid());
    const Graph graph(env.grid());
    const auto distance = [&] {
        return bfs_path(graph, env.guard_position(), env.player_position()).cost;
    };

    // El jugador espera; el vigilante (a 6 pasos) se acerca uno cada turno par.
    Position before = env.guard_position();
    int d = distance();
    for (std::size_t turn = 1; turn <= 4; ++turn) {
        (void)play_turn(env, Action::wait, guard.get());
        assert(env.turn() == turn);
        if (turn % 2 == 1) {
            assert(env.guard_position() == before);
            assert(distance() == d);
        } else {
            assert(manhattanDistance(before, env.guard_position()) == 1);
            assert(isTraversable(env.grid().at(env.guard_position())));
            assert(distance() == d - 1);
            d = distance();
        }
        before = env.guard_position();
    }
}

void followsAMovingPlayer() {
    // El jugador cambia de lado del anillo; el vigilante cambia de direccion.
    NavigationEnvironment env(scenario(kRing, {3, 2}, {1, 1}, {3, 4}, {1, 3}));
    const auto guard = make_guard_controller(env.grid());
    (void)play_turn(env, Action::wait, guard.get());   // turno 1: quieto
    (void)play_turn(env, Action::wait, guard.get());   // turno 2: hacia (3,2) por la izquierda
    assert((env.guard_position() == Position{1, 2}));
    (void)play_turn(env, Action::right, guard.get());  // turno 3: jugador (3,3)
    (void)play_turn(env, Action::right, guard.get());  // turno 4: jugador (3,4) = B
    // Desde (1,2) hacia (3,4) ambos lados cuestan 6 pasos: empate, y en el orden
    // fijo (arriba, abajo, izquierda, derecha) gana la izquierda.
    assert((env.guard_position() == Position{1, 1}));
    (void)play_turn(env, Action::right, guard.get());  // turno 5: jugador (3,5) = salida
    (void)play_turn(env, Action::up, guard.get());     // turno 6: jugador (2,5)
    // Ahora el jugador esta mas cerca por la derecha: el vigilante da la vuelta.
    assert((env.guard_position() == Position{1, 2}));
}

void waitsWithoutPath() {
    // El vigilante esta encerrado: nunca hay camino hacia el jugador.
    const std::vector<std::string> rows = {
        "#########",
        "#...S#..#",
        "#########",
    };
    NavigationEnvironment env(scenario(rows, {1, 1}, {1, 2}, {1, 3}, {1, 6}));
    const auto guard = make_guard_controller(env.grid());
    for (int i = 0; i < 4; ++i) (void)play_turn(env, Action::wait, guard.get());
    assert((env.guard_position() == Position{1, 6}));
    assert(!env.isFinished());
}

void captureByGuardMove() {
    NavigationEnvironment env(scenario(kRing, {1, 1}, {3, 1}, {3, 3}, {1, 2}));
    const auto guard = make_guard_controller(env.grid());
    (void)play_turn(env, Action::wait, guard.get());  // turno 1: quieto, adyacente
    const auto end = play_turn(env, Action::wait, guard.get());
    assert(end.finished);
    assert(end.reason == EndReason::captured);
    assert(env.guard_position() == env.player_position());
}

void captureAtExitBeatsEscape() {
    // Pasillo: A (1,2), B (1,3), salida (1,5), vigilante (1,6) pegado a la salida.
    const std::vector<std::string> rows = {
        "########",
        "#....S.#",
        "########",
    };
    NavigationEnvironment env(scenario(rows, {1, 1}, {1, 2}, {1, 3}, {1, 6}));
    const auto guard = make_guard_controller(env.grid());
    (void)play_turn(env, Action::right, guard.get());  // A
    (void)play_turn(env, Action::right, guard.get());  // B; el vigilante entra a la salida
    assert((env.guard_position() == Position{1, 5}));
    (void)play_turn(env, Action::right, guard.get());  // (1,4)
    const auto end = play_turn(env, Action::right, guard.get());  // pisa la salida con A y B
    assert(env.is_switch_active(SwitchId::a) && env.is_switch_active(SwitchId::b));
    assert(end.reason == EndReason::captured);
}

void playTurnMatchesLegacyStepWithoutGuard() {
    NavigationEnvironment a(scenario(kRing, {3, 1}, {1, 1}, {3, 4}, {1, 5}));
    NavigationEnvironment b(scenario(kRing, {3, 1}, {1, 1}, {3, 4}, {1, 5}));
    for (Action act : {Action::right, Action::wait, Action::up, Action::left}) {
        const auto ra = a.step(act);
        const auto rb = play_turn(b, act, nullptr);
        assert(ra.events.size() == rb.events.size());
        assert(ra.reason == rb.reason);
        assert(a.player_position() == b.player_position());
        assert(a.guard_position() == b.guard_position());
        assert(a.energy() == b.energy());
    }
}

void sameGameSameResult() {
    const auto run = [] {
        NavigationEnvironment env(scenario(kRing, {3, 1}, {1, 1}, {3, 4}, {1, 5}));
        const auto guard = make_guard_controller(env.grid());
        std::vector<Position> trace;
        for (Action act : {Action::right, Action::right, Action::up, Action::wait,
                           Action::left, Action::down}) {
            if (env.isFinished()) break;
            (void)play_turn(env, act, guard.get());
            trace.push_back(env.guard_position());
        }
        return trace;
    };
    assert(run() == run());
}

}  // namespace

int main() {
    recalculatesTowardCurrentPlayer();
    respectsWallsAndFixedTieBreak();
    waitsWhenAlreadyThereOrIllegal();
    movesOnlyOnEvenTurnsAlongShortestPath();
    followsAMovingPlayer();
    waitsWithoutPath();
    captureByGuardMove();
    captureAtExitBeatsEscape();
    playTurnMatchesLegacyStepWithoutGuard();
    sameGameSameResult();
    return 0;
}
