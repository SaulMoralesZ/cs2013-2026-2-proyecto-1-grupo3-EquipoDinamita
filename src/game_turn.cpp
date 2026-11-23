#include "../include/circuit_escape/game_turn.hpp"

#include <iterator>
#include <utility>
#include <vector>

namespace circuit_escape {

StepResult play_turn(NavigationEnvironment& environment, Action playerAction,
                     IController* guard) {
    std::vector<NavigationEvent> events;
    const auto append = [&events](StepResult& part) {
        events.insert(events.end(), std::make_move_iterator(part.events.begin()),
                      std::make_move_iterator(part.events.end()));
    };

    StepResult player = environment.step_player(playerAction);
    append(player);

    if (!player.finished && environment.guard_moves_this_turn()) {
        Action guardAction = Action::wait;
        if (guard) {
            const auto legal = environment.guard_legal_actions();
            guardAction = guard->selectAction(environment.state(), legal);
        }
        StepResult guardStep = environment.step_guard(guardAction);
        append(guardStep);
    }

    StepResult last = environment.finish_turn();
    append(last);
    return StepResult{std::move(last.observation), std::move(events), last.finished,
                      last.reason};
}

}  // namespace circuit_escape
