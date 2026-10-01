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

namespace circuit_escape {

// -------------------------------------------------- Observación y eventos
struct Observation {
    Position agent;
    Position goal;
    int energy{};
    int maximumEnergy{};
    int score{};
    std::size_t collectedResources{};
    std::size_t turn{};
    std::vector<Action> availableActions;
};

struct MovedEvent { Position from; Position to; int energyCost; };
struct MovementRejectedEvent { Position from; Action action; };
struct ResourceCollectedEvent { Position at; int points; };
struct EnergyChangedEvent { int previous; int current; };
struct TrapTriggeredEvent { Position at; };
struct GoalReachedEvent { Position at; };

using NavigationEvent = std::variant<
    MovedEvent,
    MovementRejectedEvent,
    ResourceCollectedEvent,
    EnergyChangedEvent,
    TrapTriggeredEvent,
    GoalReachedEvent>;

enum class EndReason { none, goalReached, noEnergy, turnLimit };

struct StepResult {
    Observation observation;
    std::vector<NavigationEvent> events;
    bool finished{false};
    EndReason reason{EndReason::none};
};

// Texto de diagnóstico / interfaz (implementados en environment.cpp).
[[nodiscard]] std::string describe(const NavigationEvent& event);
[[nodiscard]] std::string_view toString(EndReason reason) noexcept;
[[nodiscard]] std::vector<std::string> readLines(const std::filesystem::path& path);

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

// --------------------------------------------------------- Escenario
template<std::size_t Rows, std::size_t Columns>
struct Scenario {
    Grid<Cell, Rows, Columns> grid{};
    Position start{};
};

template<std::size_t Rows, std::size_t Columns>
[[nodiscard]] Scenario<Rows, Columns> parseScenario(std::span<const std::string> lines) {
    if (lines.size() != Rows) {
        throw std::invalid_argument("Escenario: se esperaban " + std::to_string(Rows) +
                                    " filas y hay " + std::to_string(lines.size()));
    }
    Scenario<Rows, Columns> scenario{};
    std::optional<Position> start;
    for (std::size_t r = 0; r < Rows; ++r) {
        if (lines[r].size() != Columns) {
            throw std::invalid_argument("Escenario: la fila " + std::to_string(r) +
                                        " debe tener " + std::to_string(Columns) +
                                        " columnas");
        }
        for (std::size_t c = 0; c < Columns; ++c) {
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
template<std::size_t Rows, std::size_t Columns>
class NavigationEnvironment {
public:
    using grid_type = Grid<Cell, Rows, Columns>;

    NavigationEnvironment(grid_type initialGrid, Position start,
                          GameRules rules = rulesFor(Difficulty::standard))
        : initialGrid_(std::move(initialGrid)),
          grid_(initialGrid_),
          start_(start),
          rules_(rules) {
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

    // Restaura la partida inicial. La semilla se conserva para los
    // controladores / experimentos (el entorno es determinista).
    void reset(std::uint32_t seed) {
        grid_ = initialGrid_;
        agent_ = start_;
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
            const int cost = entryCost(cell);
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
    // Posición destino si el movimiento es válido (dentro del tablero y transitable).
    [[nodiscard]] std::optional<Position> target(Action action) const {
        const auto candidate = neighbor(agent_, action);
        if (!candidate || !grid_.contains(*candidate)) return std::nullopt;
        if (!isTraversable(grid_.at(*candidate))) return std::nullopt;
        return candidate;
    }

    [[nodiscard]] int entryCost(const Cell& cell) const {
        return std::visit(
            Overloaded{
                [&](const RoughTerrain&) { return rules_.roughCost; },
                [&](const auto&) { return rules_.baseCost; }},
            cell);
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

    Position agent_{};
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