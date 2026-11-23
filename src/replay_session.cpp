#include "../include/circuit_escape/replay_session.hpp"

#include <stdexcept>
#include <utility>

#include "../include/circuit_escape/controllers/guard_controller.hpp"
#include "../include/circuit_escape/game_turn.hpp"
#include "../include/circuit_escape/map_io/scenario_format.hpp"

namespace circuit_escape {

ReplaySession::ReplaySession(const Map& map) : ReplaySession(map, map.winning_play) {}

ReplaySession::ReplaySession(const Map& map, std::vector<Action> actions)
    : env_(to_scenario(map)),
      guard_(make_guard_controller(env_.grid())),
      actions_(std::move(actions)) {}

bool ReplaySession::step() {
    if (finished()) return false;
    events_ = play_turn(env_, actions_[next_], guard_.get()).events;
    ++next_;
    return true;
}

void ReplaySession::restart() {
    env_.reset(env_.seed());
    next_ = 0;
    events_.clear();
}

bool ReplaySession::finished() const noexcept {
    return env_.isFinished() || next_ >= actions_.size();
}

Action ReplaySession::next_action() const {
    if (finished()) throw std::logic_error("ReplaySession: no quedan acciones");
    return actions_[next_];
}

}  // namespace circuit_escape
