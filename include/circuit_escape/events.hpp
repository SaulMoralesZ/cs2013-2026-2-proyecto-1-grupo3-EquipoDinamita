#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "position.hpp"

namespace circuit_escape {

// Copia inmutable de lo que un controlador puede observar.
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

// Eventos: describen algo que ya ocurrió.
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

// Combina varios callables en un único visitante para std::visit.
template<typename... Callables>
struct Overloaded : Callables... {
    using Callables::operator()...;
};

template<typename... Callables>
Overloaded(Callables...) -> Overloaded<Callables...>;

// Publica varios eventos, en orden, con una fold expression.
template<typename... Events>
void publish(std::vector<NavigationEvent>& sink, Events&&... events) {
    (sink.emplace_back(std::forward<Events>(events)), ...);
}

// Texto en español para diagnóstico y para la interfaz (sin emojis).
[[nodiscard]] std::string describe(const NavigationEvent& event);
[[nodiscard]] std::string toString(EndReason reason);
[[nodiscard]] std::string describe(EndReason reason);

}  // namespace circuit_escape
