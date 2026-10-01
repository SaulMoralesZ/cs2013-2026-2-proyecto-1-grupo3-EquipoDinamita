#include "../include/circuit_escape/controllers.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace circuit_escape {

namespace {

std::vector<Action> movesOnly(std::span<const Action> legalActions) {
    std::vector<Action> moves;
    std::copy_if(legalActions.begin(), legalActions.end(), std::back_inserter(moves),
                 [](Action a) { return a != Action::wait; });
    return moves;
}

}  // namespace

Action RandomPolicy::selectAction(const Observation&,
                                  std::span<const Action> legalActions) {
    const auto moves = movesOnly(legalActions);
    if (moves.empty()) return Action::wait;
    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
    return moves[pick(rng_)];
}

Action HeuristicPolicy::selectAction(const Observation& observation,
                                     std::span<const Action> legalActions) {
    ++visits_[observation.agent];
    const auto moves = movesOnly(legalActions);

    const auto score = [&](Action action) {
        const Position next = neighbor(observation.agent, action).value_or(observation.agent);
        const auto it = visits_.find(next);
        const int visited = it == visits_.end() ? 0 : it->second;
        return -static_cast<int>(manhattan(next, observation.goal)) - 2 * visited;
    };

    return bestAction(moves.begin(), moves.end(), score).value_or(Action::wait);
}

Action HumanController::selectAction(const Observation&, std::span<const Action>) {
    if (!pending_) {
        throw std::logic_error("HumanController: no hay una accion pendiente");
    }
    const Action action = *pending_;
    pending_.reset();
    return action;
}

std::optional<ControllerKind> parseControllerKind(std::string_view name) {
    if (name == "random") return ControllerKind::random;
    if (name == "heuristic") return ControllerKind::heuristic;
    return std::nullopt;
}

std::string_view toString(ControllerKind kind) noexcept {
    switch (kind) {
        case ControllerKind::random: return "random";
        case ControllerKind::heuristic: return "heuristic";
    }
    return "random";
}

std::unique_ptr<IController> makeController(ControllerKind kind, std::uint32_t seed) {
    switch (kind) {
        case ControllerKind::random:
            return std::make_unique<PolicyController<RandomPolicy>>(RandomPolicy{seed});
        case ControllerKind::heuristic:
            return std::make_unique<PolicyController<HeuristicPolicy>>(HeuristicPolicy{});
    }
    throw std::invalid_argument("ControllerKind desconocido");
}

}  // namespace circuit_escape