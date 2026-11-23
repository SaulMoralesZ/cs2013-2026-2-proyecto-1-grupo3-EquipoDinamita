#include "../../include/circuit_escape/map_generation/winning_play.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>

#include "../../include/circuit_escape/environment.hpp"
#include "../../include/circuit_escape/graph/graph.hpp"
#include "../../include/circuit_escape/pathfinding/bfs.hpp"
#include "../../include/circuit_escape/pathfinding/dijkstra.hpp"
#include "../../include/circuit_escape/replay_session.hpp"

namespace circuit_escape {

// ------------------------------------------------------------- Reproduccion
namespace {

std::size_t pending_batteries(const Grid<Cell>& grid) {
    return countCells(grid.begin(), grid.end(), [](const Cell& cell) {
        const auto* battery = std::get_if<Battery>(&cell);
        return battery && !battery->consumed;
    });
}

}  // namespace

ReplayOutcome replay_winning_play(const Map& map, const std::vector<Action>& actions) {
    ReplayOutcome out;
    try {
        // Misma reproduccion que la pantalla "Ver victoria guardada" del juego.
        ReplaySession session(map, actions);
        const std::size_t before = pending_batteries(session.environment().grid());
        while (session.step()) {}
        const NavigationEnvironment& env = session.environment();
        out.actions_played = session.played();
        out.reason = env.endReason();
        out.won = out.reason == EndReason::goalReached;
        out.final_energy = env.energy();
        out.turns = env.turn();
        out.batteries_used = before - pending_batteries(env.grid());
    } catch (const std::exception& error) {
        out.error = error.what();
    }
    return out;
}

// ------------------------------------------------------------------ Busqueda
namespace {

constexpr int kNone = -1;
constexpr int kInfinite = std::numeric_limits<int>::max() / 4;
constexpr std::array<Action, 4> kMoves{Action::up, Action::down, Action::left, Action::right};
enum Slot : std::size_t { kA = 0, kB = 1, kExit = 2 };

// Datos precalculados del mapa (solo lectura durante la busqueda).
struct Tables {
    std::size_t vertices{0};
    std::vector<std::array<int, 4>> move;  // vecino por accion o kNone
    std::vector<int> cost;                 // entry_cost de cada vertice
    std::vector<int> battery;              // indice de bateria o kNone
    std::vector<bool> trap;
    std::array<int, 3> target{};           // vertices de A, B y salida
    int start{kNone};
    int guard{kNone};
    // Cotas inferiores hacia cada objetivo (A, B, salida) desde cada vertice.
    std::array<std::vector<int>, 3> moves_to;  // movimientos (BFS == Dijkstra unitario)
    std::array<std::vector<int>, 3> cost_to;   // energia de ir de u al objetivo
};

int clampEnergy(int value, const GameRules& rules) {
    return std::clamp(value, 0, rules.maximumEnergy);
}

int sum3(int x, int y, int z) {
    return (x >= kInfinite || y >= kInfinite || z >= kInfinite) ? kInfinite : x + y + z;
}

// Cotas inferiores (movimientos, energia) para terminar desde u.
std::pair<int, int> remaining(const Tables& t, int u, bool a, bool b) {
    const auto m = [&](std::size_t to, int from) { return t.moves_to[to][static_cast<std::size_t>(from)]; };
    const auto c = [&](std::size_t to, int from) { return t.cost_to[to][static_cast<std::size_t>(from)]; };
    const int A = t.target[kA], B = t.target[kB];
    if (!a && !b) {
        const int moves = std::min(sum3(m(kA, u), m(kB, A), m(kExit, B)),
                                   sum3(m(kB, u), m(kA, B), m(kExit, A)));
        const int energy = std::min(sum3(c(kA, u), c(kB, A), c(kExit, B)),
                                    sum3(c(kB, u), c(kA, B), c(kExit, A)));
        return {moves, energy};
    }
    if (!a) return {sum3(m(kA, u), m(kExit, A), 0), sum3(c(kA, u), c(kExit, A), 0)};
    if (!b) return {sum3(m(kB, u), m(kExit, B), 0), sum3(c(kB, u), c(kExit, B), 0)};
    return {m(kExit, u), c(kExit, u)};
}

Tables build_tables(const Map& map, const Graph& costGraph, const Graph& unitGraph) {
    Tables t;
    t.vertices = costGraph.vertexCount();
    t.move.assign(t.vertices, {kNone, kNone, kNone, kNone});
    t.cost.resize(t.vertices);
    t.battery.assign(t.vertices, kNone);
    t.trap.assign(t.vertices, false);

    int batteries = 0;
    for (std::size_t v = 0; v < t.vertices; ++v) {
        const Position p = costGraph.position(v);
        t.cost[v] = costGraph.entry_cost(v);
        for (std::size_t k = 0; k < kMoves.size(); ++k) {
            if (const auto q = map.grid.neighbor(p, kMoves[k])) t.move[v][k] = costGraph.indexOf(*q);
        }
        const Cell& cell = map.grid.at(p);
        if (const auto* bat = std::get_if<Battery>(&cell); bat && !bat->consumed) {
            t.battery[v] = batteries++;
        }
        t.trap[v] = std::holds_alternative<Trap>(cell);
    }
    if (batteries > 64) throw std::invalid_argument("demasiadas baterias para la busqueda (max 64)");

    t.target = {costGraph.indexOf(map.switchA), costGraph.indexOf(map.switchB),
                costGraph.indexOf(map.exit)};
    t.start = costGraph.indexOf(map.start);
    t.guard = costGraph.indexOf(map.vigilanteStart);

    for (std::size_t s = 0; s < 3; ++s) {
        t.moves_to[s].assign(t.vertices, kInfinite);
        t.cost_to[s].assign(t.vertices, kInfinite);
        if (t.target[s] < 0) continue;
        const Position goal = costGraph.position(static_cast<std::size_t>(t.target[s]));
        // Los movimientos son simetricos. La energia no: ir de u al objetivo
        // cuesta lo mismo que volver, cambiando que extremo se paga:
        //   costo(u -> T) = costo(T -> u) - entry(u) + entry(T).
        const ShortestPaths unit = dijkstra(unitGraph, goal);
        const ShortestPaths energy = dijkstra(costGraph, goal);
        const int goalCost = t.cost[static_cast<std::size_t>(t.target[s])];
        for (std::size_t v = 0; v < t.vertices; ++v) {
            const Position p = costGraph.position(v);
            if (const auto d = unit.cost_to(p)) t.moves_to[s][v] = *d;
            if (const auto d = energy.cost_to(p)) t.cost_to[s][v] = *d - t.cost[v] + goalCost;
        }
    }
    return t;
}

struct Node {
    int player;
    int guard;
    bool a;
    bool b;
    std::uint64_t batteries;  // bit i = bateria i ya consumida
    int energy;
    std::size_t turn;
    int parent;
    Action action;
};

struct Key {
    std::uint64_t positions;  // jugador, vigilante, interruptores y paridad
    std::uint64_t batteries;
    bool operator==(const Key&) const = default;
};

struct KeyHash {
    std::size_t operator()(const Key& k) const noexcept {
        return std::hash<std::uint64_t>{}(k.positions * 0x9e3779b97f4a7c15ULL ^ k.batteries);
    }
};

Key key_of(const Node& n) {
    const auto pos = (static_cast<std::uint64_t>(n.player) << 20) |
                     (static_cast<std::uint64_t>(n.guard) << 4) |
                     (static_cast<std::uint64_t>(n.a) << 3) |
                     (static_cast<std::uint64_t>(n.b) << 2) | (n.turn % 2);
    return Key{pos, n.batteries};
}

}  // namespace

WinningPlayResult find_winning_play(const Map& map, const WinningPlayLimits& limits) {
    WinningPlayResult result;
    const GameRules& rules = map.rules;

    GameRules unitRules = rules;
    unitRules.baseCost = 1;
    unitRules.roughCost = 1;
    const Graph costGraph(map.grid, rules);
    const Graph unitGraph(map.grid, unitRules);

    Tables t;
    try {
        t = build_tables(map, costGraph, unitGraph);
    } catch (const std::exception& error) {
        result.reason = error.what();
        return result;
    }
    if (t.start < 0 || t.guard < 0 || t.target[kA] < 0 || t.target[kB] < 0 ||
        t.target[kExit] < 0) {
        result.reason = "una posicion del escenario no es transitable";
        return result;
    }
    const int totalBatteries =
        static_cast<int>(std::count_if(t.battery.begin(), t.battery.end(),
                                       [](int b) { return b != kNone; }));

    // Decision del vigilante: la misma funcion BFS que usa GuardPolicy.
    std::unordered_map<std::uint64_t, Action> guardMemo;
    const auto guardAction = [&](int guard, int player) {
        const auto k = static_cast<std::uint64_t>(guard) * t.vertices + static_cast<std::uint64_t>(player);
        if (const auto it = guardMemo.find(k); it != guardMemo.end()) return it->second;
        const Action a = bfs_next_action(costGraph, costGraph.position(static_cast<std::size_t>(guard)),
                                         costGraph.position(static_cast<std::size_t>(player)))
                             .value_or(Action::wait);
        guardMemo.emplace(k, a);
        return a;
    };

    std::vector<Node> nodes;
    // Frente de Pareto por estado: pares (energia, turno) no dominados.
    // Con el mismo jugador, vigilante, interruptores, baterias y paridad,
    // mas energia y un turno anterior nunca es peor -> se puede descartar
    // lo dominado sin perder ninguna victoria.
    using Record = std::pair<int, std::size_t>;
    std::unordered_map<Key, std::vector<Record>, KeyHash> front;
    const auto dominates = [](const Record& r, int energy, std::size_t turn) {
        return r.first >= energy && r.second <= turn;
    };
    // (f, -energia, id): menor f, luego mas energia, luego el mas antiguo.
    using Entry = std::tuple<std::size_t, int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

    const auto push = [&](Node node, std::size_t f) {
        auto& records = front[key_of(node)];
        for (const Record& r : records) {
            if (dominates(r, node.energy, node.turn)) return;
        }
        std::erase_if(records, [&](const Record& r) {
            return node.energy >= r.first && node.turn <= r.second;
        });
        records.emplace_back(node.energy, node.turn);
        nodes.push_back(node);
        open.emplace(f, -node.energy, static_cast<int>(nodes.size() - 1));
    };

    {
        Node root{t.start, t.guard, false, false, 0, rules.initialEnergy, 0, kNone, Action::wait};
        const auto [hm, hc] = remaining(t, root.player, false, false);
        if (hm >= kInfinite) {
            result.reason = "no hay recorrido completo";
            return result;
        }
        push(root, static_cast<std::size_t>(hm));
    }

    const auto reconstruct = [&](int index, Action last) {
        std::vector<Action> actions{last};
        for (int i = index; i >= 0 && nodes[static_cast<std::size_t>(i)].parent != kNone;
             i = nodes[static_cast<std::size_t>(i)].parent) {
            actions.push_back(nodes[static_cast<std::size_t>(i)].action);
        }
        std::reverse(actions.begin(), actions.end());
        return actions;
    };

    std::optional<std::vector<Action>> found;
    while (!open.empty() && !found) {
        const auto [f, negEnergy, index] = open.top();
        open.pop();
        const Node node = nodes[static_cast<std::size_t>(index)];
        {   // Superado despues de encolarse: ya no esta en el frente.
            const auto& records = front[key_of(node)];
            const bool alive = std::any_of(records.begin(), records.end(), [&](const Record& r) {
                return r.first == node.energy && r.second == node.turn;
            });
            if (!alive) continue;
        }
        if (++result.expanded > limits.max_expanded) break;

        for (std::size_t k = 0; k <= kMoves.size() && !found; ++k) {
            const Action action = k < kMoves.size() ? kMoves[k] : Action::wait;
            Node next = node;
            next.parent = index;
            next.action = action;
            next.turn = node.turn + 1;

            // 1. Actua el jugador (mismas reglas que NavigationEnvironment).
            if (action == Action::wait) {
                next.energy = clampEnergy(next.energy - rules.waitCost, rules);
            } else {
                const int q = t.move[static_cast<std::size_t>(node.player)][k];
                if (q == kNone) continue;  // movimiento invalido: nunca conviene
                const auto qi = static_cast<std::size_t>(q);
                next.player = q;
                next.energy = clampEnergy(next.energy - t.cost[qi], rules);
                if (const int bat = t.battery[qi];
                    bat != kNone && !(next.batteries & (std::uint64_t{1} << bat))) {
                    next.batteries |= std::uint64_t{1} << bat;
                    next.energy = clampEnergy(next.energy + rules.batteryRecharge, rules);
                }
                if (t.trap[qi]) next.energy = clampEnergy(next.energy - rules.trapEnergyPenalty, rules);
            }
            // 2. Captura inicial.
            if (next.player == node.guard) continue;
            // 3. Vigilante en turnos pares.
            if (next.turn % 2 == 0) {
                const Action g = guardAction(node.guard, next.player);
                if (g != Action::wait) {
                    const auto gk = static_cast<std::size_t>(
                        std::find(kMoves.begin(), kMoves.end(), g) - kMoves.begin());
                    next.guard = t.move[static_cast<std::size_t>(node.guard)][gk];
                }
            }
            // 4. Resultado del turno.
            if (next.player == next.guard || next.energy <= 0) continue;
            if (next.player == t.target[kA]) next.a = true;
            if (next.player == t.target[kB]) next.b = true;
            if (next.player == t.target[kExit] && next.a && next.b) {
                found = reconstruct(index, action);
                break;
            }
            if (next.turn >= rules.turnLimit) continue;

            // Podas con cotas inferiores.
            const auto [hm, hc] = remaining(t, next.player, next.a, next.b);
            if (hm >= kInfinite || next.turn + static_cast<std::size_t>(hm) > rules.turnLimit) continue;
            const int pendingBatteries = totalBatteries - std::popcount(next.batteries);
            if (next.energy + rules.batteryRecharge * pendingBatteries - hc <= 0) continue;

            push(next, next.turn + static_cast<std::size_t>(hm));
        }
    }

    if (!found) {
        result.reason = result.expanded > limits.max_expanded
                            ? "busqueda agotada (" + std::to_string(limits.max_expanded) +
                                  " estados) sin encontrar victoria"
                            : "no existe partida ganadora: el vigilante, la energia o el "
                              "limite de turnos lo impiden";
        return result;
    }

    // La autoridad es el motor: se reproduce la secuencia con las reglas reales.
    result.replay = replay_winning_play(map, *found);
    if (!result.replay.won) {
        result.reason = "la secuencia encontrada no gana al reproducirla con el motor (" +
                        (result.replay.error.empty() ? toString(result.replay.reason)
                                                     : result.replay.error) +
                        ")";
        return result;
    }
    result.found = true;
    result.actions = std::move(*found);
    return result;
}

}  // namespace circuit_escape
