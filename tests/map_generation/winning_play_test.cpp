#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "circuit_escape/map_generation/map_builder.hpp"
#include "circuit_escape/map_generation/map_validator.hpp"
#include "circuit_escape/map_generation/winning_play.hpp"

using namespace circuit_escape;

namespace {

// '#' muro, 'S' salida, 'B' bateria; el resto libre.
Map mapFrom(const std::vector<std::string>& rows, Position start, Position a, Position b,
            Position guard, int energy, std::size_t turnLimit) {
    Map map;
    map.grid = Grid<Cell>(rows.size(), rows.front().size());
    for (std::size_t r = 0; r < rows.size(); ++r)
        for (std::size_t c = 0; c < rows[r].size(); ++c) {
            const char ch = rows[r][c];
            map.grid.at({r, c}) = ch == '#'   ? Cell{Wall{}}
                                  : ch == 'S' ? Cell{Exit{}}
                                  : ch == 'B' ? Cell{Battery{}}
                                              : Cell{Empty{}};
            if (ch == 'S') map.exit = {r, c};
        }
    map.start = start;
    map.switchA = a;
    map.switchB = b;
    map.vigilanteStart = guard;
    map.rules = rulesFor(Difficulty::standard);
    map.rules.initialEnergy = energy;
    map.rules.maximumEnergy = std::max(energy, 10);
    map.rules.turnLimit = turnLimit;
    map.rules.batteryRecharge = 3;
    return map;
}

// Pasillo: vigilante detras del jugador; A y B por delante, salida al final.
//   ##########
//   #G@AB....S#   (columnas 1..9)
Map corridor(int energy = 60, std::size_t turnLimit = 50, bool battery = false) {
    const std::string middle = battery ? "#....B...S#" : "#........S#";
    return mapFrom({"###########", middle, "###########"}, {1, 2}, {1, 3}, {1, 4}, {1, 1},
                   energy, turnLimit);
}

void knownWinningSequence() {
    const WinningPlayResult r = find_winning_play(corridor());
    assert(r.found);
    // El camino mas corto: 7 pasos a la derecha (el vigilante va a media velocidad).
    assert(r.actions == std::vector<Action>(7, Action::right));
    assert(r.replay.won);
    assert(r.replay.turns == 7);
    assert(r.replay.final_energy == 60 - 7);
    assert(r.replay.batteries_used == 0);
}

void deterministic() {
    MapConfig config;
    config.numBatteries = 2;
    config.numTraps = 2;
    const Map map = MapBuilder::buildMap(1, 11, 10, 15, config);
    const WinningPlayResult first = find_winning_play(map);
    const WinningPlayResult again = find_winning_play(map);
    assert(first.found == again.found);
    assert(first.actions == again.actions);
}

void rechargeIsUsedWhenNeeded() {
    // Costo 7 y energia 5: sin la bateria no se gana (seccion 2.3).
    const Map map = corridor(5, 50, true);
    const MapValidation v = validate_map(map);
    assert(v.status == ValidationStatus::needs_recharge_proof);
    const WinningPlayResult r = find_winning_play(map);
    assert(r.found);
    assert(r.replay.batteries_used == 1);
    assert(r.replay.final_energy == 1);  // 5 - 7 + 3
}

void turnLimitTooShort() {
    const WinningPlayResult r = find_winning_play(corridor(60, 6));  // hacen falta 7
    assert(!r.found);
    assert(!r.reason.empty());
}

void lastAllowedTurnCounts() {
    const WinningPlayResult r = find_winning_play(corridor(60, 7));
    assert(r.found && r.replay.turns == 7);
}

void guardBlocksTheOnlyWay() {
    // El vigilante tapa el pasillo entre el jugador y los objetivos.
    const Map map = mapFrom({"##########", "#@.G.AB.S#", "##########"}, {1, 1}, {1, 5}, {1, 6},
                            {1, 3}, 60, 100);
    const WinningPlayResult r = find_winning_play(map);
    assert(!r.found);
}

void replayRejectsWrongSequences() {
    const Map map = corridor();
    assert(!replay_winning_play(map, std::vector<Action>(6, Action::right)).won);  // no llega
    const ReplayOutcome back = replay_winning_play(map, {Action::left});           // al vigilante
    assert(!back.won && back.reason == EndReason::captured);
}

void generatedMapsHaveVerifiedWins() {
    MapConfig config;
    config.numResources = 2;
    config.numBatteries = 2;
    config.numTraps = 2;
    int wins = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        const Map map = MapBuilder::buildMap(1, seed, 10, 15, config);
        const WinningPlayResult r = find_winning_play(map);
        if (!r.found) continue;
        ++wins;
        // Lo guardado se reproduce con el motor real.
        assert(replay_winning_play(map, r.actions).won);
    }
    assert(wins > 0);
}

}  // namespace

int main() {
    knownWinningSequence();
    deterministic();
    rechargeIsUsedWhenNeeded();
    turnLimitTooShort();
    lastAllowedTurnCounts();
    guardBlocksTheOnlyWay();
    replayRejectsWrongSequences();
    generatedMapsHaveVerifiedWins();
    return 0;
}
