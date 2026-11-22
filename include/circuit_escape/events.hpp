#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "cells.hpp"
#include "grid.hpp"
#include "position.hpp"

namespace circuit_escape {

enum class SwitchId {a,b};

struct Observation {
    Position agent;
    Position goal;
    int energy{};
    int maximumEnergy{};
    int score{};
    std::size_t collectedResources{};
    std::size_t turn{};
    std::vector<Action> availableActions;
    const Grid<Cell>* map{nullptr};
    bool switch_a_active{false};
    bool switch_b_active{false};
    Position guard{};
};

struct MovedEvent {
    Position from;
    Position to;
    int energyCost;
};

struct MovementRejectedEvent {
    Position from;
    Action action;
};

struct ResourceCollectedEvent {
    Position at;
    int points;
};

struct EnergyChangedEvent {
    int previous;
    int current;
};

struct TrapTriggeredEvent {
    Position at;
};

struct GoalReachedEvent {
    Position at;
};

struct GuardMovedEvent {
    Position from;
    Position to;
};

struct SwitchActivatedEvent {
    SwitchId id;
    Position at;
};

struct CapturedEvent {
    Position at;
};

using NavigationEvent = std::variant<
    MovedEvent,
    MovementRejectedEvent,
    ResourceCollectedEvent,
    EnergyChangedEvent,
    TrapTriggeredEvent,
    GoalReachedEvent,
    GuardMovedEvent,
    SwitchActivatedEvent,
    CapturedEvent
>;

enum class EndReason {
    none,
    goalReached,
    noEnergy,
    turnLimit,
    captured
};

struct StepResult {
    Observation observation;
    std::vector<NavigationEvent> events;
    bool finished{false};
    EndReason reason{EndReason::none};
};

std::string describe(const NavigationEvent& event);

std::string toString(EndReason reason);

} // namespace circuit_escape