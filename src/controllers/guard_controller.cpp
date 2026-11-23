#include "../../include/circuit_escape/controllers/guard_controller.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "../../include/circuit_escape/pathfinding/bfs.hpp"

namespace circuit_escape {

GuardPolicy::GuardPolicy(std::shared_ptr<const Graph> graph) : graph_(std::move(graph)) {
    if (!graph_) throw std::invalid_argument("GuardPolicy: el grafo no puede ser nulo");
}

Action GuardPolicy::selectAction(const Observation& observation,
                                 std::span<const Action> legalActions) {
    const auto step = bfs_next_action(*graph_, observation.guard, observation.agent);
    if (!step) return Action::wait;
    // Defensa: solo se propone una accion que el motor declaro legal.
    const bool legal =
        std::find(legalActions.begin(), legalActions.end(), *step) != legalActions.end();
    return legal ? *step : Action::wait;
}

std::unique_ptr<IController> make_guard_controller(const Grid<Cell>& grid) {
    return std::make_unique<PolicyController<GuardPolicy>>(
        GuardPolicy{std::make_shared<const Graph>(grid)});
}

}  // namespace circuit_escape
