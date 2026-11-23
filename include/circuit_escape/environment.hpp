#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "./cells.hpp"
#include "./entry_cost.hpp"
#include "./game_rules.hpp"
#include "./grid.hpp"
#include "./events.hpp"
#include "./scenario.hpp"

namespace circuit_escape {

std::vector<std::string> readLines(const std::filesystem::path& path);

// ------------------------------------------- Utilidades variádicas (fold)

// Publica varios eventos en el orden dado (fold con operador coma).
template<typename... Events>
void publish(std::vector<NavigationEvent>& out, Events&&... events) {
    (out.emplace_back(std::forward<Events>(events)), ...);
}

struct EndCheck {
    bool holds;
    EndReason reason;
};

// Devuelve la razón de la primera condición verdadera (fold con ||,
// que corta en la primera). El orden de los argumentos es la precedencia.
template<typename... Checks>
[[nodiscard]] constexpr EndReason firstEnd(Checks... checks) {
    EndReason result = EndReason::none;
    [[maybe_unused]] const bool found =
        ((checks.holds ? (result = checks.reason, true) : false) || ...);
    return result;
}



// Lo que describe un archivo de texto del Proyecto 1: tablero y posición inicial ('@'), sin interruptores ni vigilante
struct BoardLayout {
    Grid<Cell> grid;
    Position start{};
};

[[nodiscard]]inline BoardLayout parseScenario(std::span<const std::string> lines) {
    if (lines.empty()) {
        throw std::invalid_argument("Escenario: no tiene filas");
    }

    const std::size_t rows = lines.size();
    const std::size_t columns = lines.front().size();

    BoardLayout scenario{Grid<Cell>(rows, columns), Position{}};
    std::optional<Position> start;
    for (std::size_t r = 0; r < rows; ++r) {
        if (lines[r].size() != columns) {
            throw std::invalid_argument("Escenario: la fila " + std::to_string(r) +
                                        " debe tener " + std::to_string(columns) +
                                        " columnas");
        }
        for (std::size_t c = 0; c < columns; ++c) {
            const auto cell = cellFromChar(lines[r][c]);
            if (!cell) {
                throw std::invalid_argument(std::string("Escenario: simbolo desconocido '") +
                                            lines[r][c] + "' en " + toString({r, c}));
            }
            scenario.grid.at({r, c}) = *cell;
            if (lines[r][c] == '@') {
                if (start) throw std::invalid_argument("Escenario: mas de un inicio '@'");
                start = Position{r, c};
            }
        }
    }
    if (!start) throw std::invalid_argument("Escenario: falta el inicio '@'");
    scenario.start = *start;
    return scenario;
}

// ----------------------------------------------------------- Entorno
class NavigationEnvironment {
    // Posiciones de los objetivos de la partida (interruptores y vigilante)
    struct Objectives {
        Position switch_a;
        Position switch_b;
        Position guard_start;
    };

    // Fase del turno en curso: obliga a llamar en el orden del PDF §3.2.
    enum class TurnPhase { idle, player_done, guard_done };

public:
    using grid_type = Grid<Cell>;
    explicit NavigationEnvironment(const Scenario& scenario)
        : NavigationEnvironment(scenario.grid, scenario.start, scenario.rules,
                                Objectives{scenario.switch_a, scenario.switch_b,
                                scenario.guard_start}) {}
    NavigationEnvironment(Grid<Cell> initialGrid, Position start,
                          GameRules rules = rulesFor(Difficulty::standard))
        : NavigationEnvironment(std::move(initialGrid), start, rules, std::nullopt) {}

private:
    NavigationEnvironment(Grid<Cell> initialGrid, Position start, GameRules rules,
                          std::optional<Objectives> objectives)
        : initialGrid_(std::move(initialGrid)),
          grid_(initialGrid_),
          start_(start),
          rules_(rules),
          objectives_(objectives) {
        if (rules_.initialEnergy <= 0 || rules_.maximumEnergy < rules_.initialEnergy ||
            rules_.turnLimit == 0) {
            throw std::invalid_argument("Reglas invalidas: energia o limite de turnos");
            }
        if (!initialGrid_.contains(start_)) {
            throw std::invalid_argument("La posicion inicial esta fuera del tablero");
        }
        if (!isTraversable(initialGrid_.at(start_))) {
            throw std::invalid_argument("La posicion inicial no es transitable");
        }
        const auto isExit = [](const Cell& c) { return std::holds_alternative<Exit>(c); };
        if (countCells(initialGrid_.begin(), initialGrid_.end(), isExit) != 1) {
            throw std::invalid_argument("El tablero debe tener exactamente una salida");
        }
        goal_ = *findPosition(initialGrid_, isExit);
        if (objectives_) validateObjectives(*objectives_);

        // La recompensa configurada se estampa en cada recurso del mapa.
        std::for_each(initialGrid_.begin(), initialGrid_.end(), [&](Cell& cell) {
            if (auto* resource = std::get_if<ResourceCell<int>>(&cell)) {
                resource->reward = rules_.resourcePoints;
            }
        });
        totalResources_ = countCells(
            initialGrid_.begin(), initialGrid_.end(),
            [](const Cell& c) { return std::holds_alternative<ResourceCell<int>>(c); });
        reset(0);
    }

public:
    // Restaura la partida inicial. La semilla se conserva para los
    // controladores / experimentos (el entorno es determinista).
    void reset(std::uint32_t seed) {
        grid_ = initialGrid_;
        agent_ = start_;
        guard_ = objectives_ ? objectives_->guard_start : Position{};
        switch_a_active_ = false;
        switch_b_active_ = false;
        phase_ = TurnPhase::idle;
        energy_ = rules_.initialEnergy;
        score_ = 0;
        collected_ = 0;
        turn_ = 0;
        finished_ = false;
        reason_ = EndReason::none;
        seed_ = seed;
    }

    [[nodiscard]] Observation state() const {
        return Observation{agent_, goal_, energy_, rules_.maximumEnergy, score_,
                           collected_, turn_, availableActions(),
                           &grid_, switch_a_active_, switch_b_active_, guard_};
    }

    [[nodiscard]] std::vector<Action> availableActions() const {
        std::vector<Action> actions;
        if (finished_) return actions;
        for (Action action : {Action::up, Action::down, Action::left, Action::right}) {
            if (target(action)) actions.push_back(action);
        }
        actions.push_back(Action::wait);
        return actions;
    }

    [[nodiscard]] bool isFinished() const noexcept { return finished_; }

    // ---- Un turno completo (PDF §3.2) = estas tres llamadas, en este orden:
    //      step_player -> (step_guard solo si guard_moves_this_turn()) -> finish_turn

    // Pasos 1 y 2: el jugador actúa y se comprueba la captura inicial.
    [[nodiscard]] StepResult step_player(Action action) {
        if (finished_) throw std::logic_error("step_player: la partida ya termino");
        if (phase_ != TurnPhase::idle) {
            throw std::logic_error("step_player: el turno anterior no se cerro con finish_turn");
        }

        ++turn_;
        std::vector<NavigationEvent> events;

        if (action == Action::wait) {
            changeEnergy(-rules_.waitCost, events);
        } else if (const auto destination = target(action)) {
            const Position from = agent_;
            agent_ = *destination;
            Cell& cell = grid_.at(agent_);
            const int cost = entry_cost(cell, rules_);
            publish(events, MovedEvent{from, agent_, cost});
            changeEnergy(-cost, events);
            interact(cell, events);
        } else {
            publish(events, MovementRejectedEvent{agent_, action});
            changeEnergy(-rules_.invalidCost, events);
        }

        // Captura inicial: el jugador entró a la casilla del vigilante.
        if (objectives_ && agent_ == guard_) {
            reason_ = EndReason::captured;
            finished_ = true;
            publish(events, CapturedEvent{agent_});
        }

        phase_ = TurnPhase::player_done;
        return StepResult{state(), std::move(events), finished_, reason_};
    }

    // Paso 3: valida y aplica la acción del vigilante. No gasta energía del
    // jugador y no recoge baterías ni recursos.
    [[nodiscard]] StepResult step_guard(Action action) {
        requireObjectives("step_guard");
        if (finished_ || phase_ != TurnPhase::player_done) {
            throw std::logic_error(
                "step_guard: debe llamarse una sola vez, justo despues de step_player, "
                "con la partida en curso");
        }
        if (!guard_moves_this_turn()) {
            throw std::logic_error("step_guard: en turnos impares el vigilante no se mueve");
        }
        const auto legal = guard_legal_actions();
        if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
            throw std::invalid_argument("step_guard: accion no permitida para el vigilante");
        }

        std::vector<NavigationEvent> events;
        if (const auto destination = destinationFrom(guard_, action)) {
            const Position from = guard_;
            guard_ = *destination;
            publish(events, GuardMovedEvent{from, guard_});
        }
        phase_ = TurnPhase::guard_done;
        return StepResult{state(), std::move(events), finished_, reason_};
    }

    // Paso 4: comprobaciones finales y cierre del turno. Orden fijo:
    // captura -> sin energia -> activar interruptor -> escape -> limite de turnos.
    // Si el turno ya terminó (captura inicial) devuelve ese resultado.
    [[nodiscard]] StepResult finish_turn() {
        if (phase_ == TurnPhase::idle) {
            throw std::logic_error("finish_turn: falta step_player");
        }
        std::vector<NavigationEvent> events;

        if (!finished_) {
            if (phase_ == TurnPhase::player_done && guard_moves_this_turn()) {
                throw std::logic_error("finish_turn: en turno par falta step_guard");
            }

            const bool captured = objectives_ && agent_ == guard_;
            reason_ = firstEnd(
                EndCheck{captured, EndReason::captured},
                EndCheck{energy_ <= 0, EndReason::noEnergy});

            if (reason_ == EndReason::captured) {
                publish(events, CapturedEvent{agent_});
            } else if (reason_ == EndReason::none) {
                activate_switch_at(agent_, events);
                reason_ = firstEnd(
                    EndCheck{agent_ == goal_ && bothSwitchesActive() && energy_ > 0,
                             EndReason::goalReached},
                    EndCheck{turn_ >= rules_.turnLimit, EndReason::turnLimit});
            }

            finished_ = reason_ != EndReason::none;
            if (reason_ == EndReason::goalReached) {
                publish(events, GoalReachedEvent{agent_});
            }
        }

        phase_ = TurnPhase::idle;
        return StepResult{state(), std::move(events), finished_, reason_};
    }

    // El vigilante se mueve en los turnos pares (los turnos se cuentan desde 1).
    // Se evalúa después de step_player.
    [[nodiscard]] bool guard_moves_this_turn() const noexcept {
        return objectives_.has_value() && turn_ != 0 && turn_ % 2 == 0;
    }

    // Movimientos legales del vigilante: respetan muros y bordes; esperar siempre.
    [[nodiscard]] std::vector<Action> guard_legal_actions() const {
        std::vector<Action> actions;
        if (!objectives_ || finished_) return actions;
        for (Action action : {Action::up, Action::down, Action::left, Action::right}) {
            if (destinationFrom(guard_, action)) actions.push_back(action);
        }
        actions.push_back(Action::wait);
        return actions;
    }

    // HEREDADO del Proyecto 1: un turno de una sola llamada. Con vigilante,
    // este siempre espera en sus turnos. Sirve a las pruebas heredadas y a main.
    [[nodiscard]] StepResult step(Action action) {
        StepResult player = step_player(action);
        std::vector<NavigationEvent> events = std::move(player.events);
        const auto append = [&events](StepResult& part) {
            events.insert(events.end(), std::make_move_iterator(part.events.begin()),
                          std::make_move_iterator(part.events.end()));
        };
        if (!finished_ && guard_moves_this_turn()) {
            StepResult guard = step_guard(Action::wait);
            append(guard);
        }
        StepResult last = finish_turn();
        append(last);
        return StepResult{std::move(last.observation), std::move(events), last.finished,
                          last.reason};
    }

    // --- Lecturas de solo lectura (nombres nuevos en snake_case) ---
    [[nodiscard]] bool has_objectives() const noexcept { return objectives_.has_value(); }
    [[nodiscard]] Position player_position() const noexcept { return agent_; }
    [[nodiscard]] int energy() const noexcept { return energy_; }
    // Turnos ya iniciados (0 tras reset; step_player lo incrementa al empezar).
    [[nodiscard]] std::size_t turn() const noexcept { return turn_; }

    [[nodiscard]] Position guard_position() const {
        requireObjectives("guard_position");
        return guard_;
    }
    [[nodiscard]] Position switch_position(SwitchId id) const {
        requireObjectives("switch_position");
        return id == SwitchId::a ? objectives_->switch_a : objectives_->switch_b;
    }
    [[nodiscard]] bool is_switch_active(SwitchId id) const {
        requireObjectives("is_switch_active");
        return id == SwitchId::a ? switch_a_active_ : switch_b_active_;
    }

    [[nodiscard]] const grid_type& grid() const noexcept { return grid_; }
    [[nodiscard]] const GameRules& rules() const noexcept { return rules_; }
    [[nodiscard]] Position start() const noexcept { return start_; }
    [[nodiscard]] Position goal() const noexcept { return goal_; }
    [[nodiscard]] EndReason endReason() const noexcept { return reason_; }
    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }
    [[nodiscard]] std::size_t totalResources() const noexcept { return totalResources_; }
    [[nodiscard]] std::size_t remainingPickups() const {
        return countCells(grid_.begin(), grid_.end(),
                          [](const Cell& c) { return isPendingPickup(c); });
    }

private:
    void requireObjectives(const char* caller) const {
        if (!objectives_) {
            throw std::logic_error(std::string(caller) +
                                   ": la partida no tiene interruptores ni vigilante");
        }
    }

    // Inicio, A, B y vigilante: dentro del tablero, sobre celda libre o de
    // terreno aspero (no Wall ni Exit) y distintos entre si.
    void validateObjectives(const Objectives& objectives) const {
        struct Named {
            Position position;
            const char* name;
        };
        const Named named[] = {{start_, "El inicio"},
                               {objectives.switch_a, "El interruptor A"},
                               {objectives.switch_b, "El interruptor B"},
                               {objectives.guard_start, "El vigilante"}};
        for (const auto& item : named) {
            if (!initialGrid_.contains(item.position)) {
                throw std::invalid_argument(std::string(item.name) +
                                            " esta fuera del tablero");
            }
            const Cell& cell = initialGrid_.at(item.position);
            if (!std::holds_alternative<Empty>(cell) &&
                !std::holds_alternative<RoughTerrain>(cell)) {
                throw std::invalid_argument(
                    std::string(item.name) +
                    " debe estar sobre una celda libre o de terreno aspero");
                }
        }
        for (std::size_t i = 0; i < std::size(named); ++i) {
            for (std::size_t j = i + 1; j < std::size(named); ++j) {
                if (named[i].position == named[j].position) {
                    throw std::invalid_argument(std::string(named[i].name) + " y " +
                                                named[j].name +
                                                " no pueden compartir casilla");
                }
            }
        }
    }

    // Posición destino desde `origin` si el movimiento es válido
    // (dentro del tablero y transitable).
    [[nodiscard]] std::optional<Position> destinationFrom(Position origin, Action action) const {
        const auto candidate = neighbor(origin, action);
        if (!candidate || !grid_.contains(*candidate)) return std::nullopt;
        if (!isTraversable(grid_.at(*candidate))) return std::nullopt;
        return candidate;
    }

    [[nodiscard]] std::optional<Position> target(Action action) const {
        return destinationFrom(agent_, action);
    }

    [[nodiscard]] bool bothSwitchesActive() const noexcept {
        return !objectives_ || (switch_a_active_ && switch_b_active_);
    }

    // Único lugar donde se activan los interruptores (decisión 6 del contrato).
    void activate_switch_at(Position position, std::vector<NavigationEvent>& events) {
        if (!objectives_) return;
        if (position == objectives_->switch_a && !switch_a_active_) {
            switch_a_active_ = true;
            publish(events, SwitchActivatedEvent{SwitchId::a, position});
        } else if (position == objectives_->switch_b && !switch_b_active_) {
            switch_b_active_ = true;
            publish(events, SwitchActivatedEvent{SwitchId::b, position});
        }
    }

    void changeEnergy(int delta, std::vector<NavigationEvent>& events) {
        const int previous = energy_;
        energy_ = std::clamp(energy_ + delta, 0, rules_.maximumEnergy);
        if (energy_ != previous) {
            publish(events, EnergyChangedEvent{previous, energy_});
        }
    }

    void interact(Cell& cell, std::vector<NavigationEvent>& events) {
        std::visit(
            Overloaded{
                [&](ResourceCell<int>& resource) {
                    if (resource.collected) return;
                    resource.collected = true;
                    score_ += resource.reward;
                    ++collected_;
                    publish(events, ResourceCollectedEvent{agent_, resource.reward});
                },
                [&](Battery& battery) {
                    if (battery.consumed) return;
                    battery.consumed = true;
                    changeEnergy(rules_.batteryRecharge, events);
                },
                [&](Trap&) {
                    score_ -= rules_.trapScorePenalty;
                    publish(events, TrapTriggeredEvent{agent_});
                    changeEnergy(-rules_.trapEnergyPenalty, events);
                },
                [](auto&) {}},
            cell);
    }

    grid_type initialGrid_;
    grid_type grid_;
    Position start_;
    Position goal_{};
    GameRules rules_;
    std::optional<Objectives> objectives_;

    Position agent_{};
    Position guard_{};
    bool switch_a_active_{false};
    bool switch_b_active_{false};
    TurnPhase phase_{TurnPhase::idle};
    int energy_{0};
    int score_{0};
    std::size_t collected_{0};
    std::size_t turn_{0};
    std::size_t totalResources_{0};
    bool finished_{false};
    EndReason reason_{EndReason::none};
    std::uint32_t seed_{0};
};

}  // namespace circuit_escape