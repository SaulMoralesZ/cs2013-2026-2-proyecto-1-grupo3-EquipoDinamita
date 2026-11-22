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
#include "./game_rules.hpp"
#include "./grid.hpp"
#include "./events.hpp"

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

// ------------------------------------------------------ Costo de entrada
// Costo de energía de entrar a una celda. Lo usan el motor, Dijkstra y la
// ayuda H, para que preparación, ayuda y juego coincidan. Solo cuentan la
// casilla base y la áspera; las recargas no son pesos negativos.
[[nodiscard]] inline int entry_cost(const Cell& cell, const GameRules& rules) {
    return std::visit(
        Overloaded{
            [&](const RoughTerrain&) { return rules.roughCost; },
            [&](const auto&) { return rules.baseCost; }},
        cell);
}

enum class SwitchId {a,b};


// --------------------------------------------------------- Escenario
//Escenario completo de una partida. La salida no es un campo: el motor la encuentra como la única celda Exit del Grid
struct Scenario {
    Grid<Cell> grid;
    Position start{};
    Position switch_a{};
    Position switch_b{};
    Position guard_start{};
    GameRules rules{};
};

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
                           collected_, turn_, availableActions()};
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

    [[nodiscard]] StepResult step(Action action) {
        if (finished_) throw std::logic_error("step: la partida ya termino");

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

        reason_ = firstEnd(
            EndCheck{agent_ == goal_ && energy_ > 0, EndReason::goalReached},
            EndCheck{energy_ <= 0, EndReason::noEnergy},
            EndCheck{turn_ >= rules_.turnLimit, EndReason::turnLimit});
        finished_ = reason_ != EndReason::none;
        if (reason_ == EndReason::goalReached) {
            publish(events, GoalReachedEvent{agent_});
        }

        return StepResult{state(), std::move(events), finished_, reason_};
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

    // Posición destino si el movimiento es válido (dentro del tablero y transitable).
    [[nodiscard]] std::optional<Position> target(Action action) const {
        const auto candidate = neighbor(agent_, action);
        if (!candidate || !grid_.contains(*candidate)) return std::nullopt;
        if (!isTraversable(grid_.at(*candidate))) return std::nullopt;
        return candidate;
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