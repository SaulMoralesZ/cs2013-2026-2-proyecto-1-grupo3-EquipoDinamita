#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "circuit_escape/controllers/guard_controller.hpp"
#include "circuit_escape/game_record.hpp"
#include "circuit_escape/game_turn.hpp"
#include "circuit_escape/map_generation/map_builder.hpp"
#include "circuit_escape/map_io/scenario_format.hpp"
#include "circuit_escape/replay_session.hpp"

using namespace circuit_escape;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / ("circuit_escape_" + name)) {
        fs::remove_all(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

Map sampleMap() {
    MapConfig config;
    config.numResources = 2;
    config.numBatteries = 2;
    config.numTraps = 2;
    Map map = MapBuilder::buildMap(4, 77, 10, 15, config);
    map.level = Difficulty::hard;
    map.minCost = 30;
    return map;
}

// Juega como lo hace GameApplication y graba una accion por turno jugado.
// Incluye esperas y movimientos que pueden chocar (tambien gastan energia).
GameRecord playAndRecord(const Map& map) {
    NavigationEnvironment env(to_scenario(map));
    const auto guard = make_guard_controller(env.grid());
    GameRecord record{map, {}, EndReason::none, 0, 0, "2026-11-23 18:40:12"};
    const std::vector<Action> plan = {Action::up,    Action::right, Action::right, Action::wait,
                                      Action::down,  Action::left,  Action::up,    Action::up,
                                      Action::right, Action::wait,  Action::down,  Action::down};
    for (Action a : plan) {
        if (env.isFinished()) break;
        (void)play_turn(env, a, guard.get());
        record.actions.push_back(a);
    }
    record.result = env.endReason();
    record.turns = env.turn();
    record.final_energy = env.energy();
    return record;
}

void roundTrip() {
    const GameRecord original = playAndRecord(sampleMap());
    const GameRecord loaded = deserialize_record(serialize_record(original));
    assert(loaded.actions == original.actions);
    assert(loaded.result == original.result);
    assert(loaded.turns == original.turns);
    assert(loaded.final_energy == original.final_energy);
    assert(loaded.date == original.date);
    // El mapa viaja completo dentro de la partida.
    assert(loaded.map.id == 4 && loaded.map.seed == 77 && loaded.map.level == Difficulty::hard);
    assert(loaded.map.rules == original.map.rules);
    for (std::size_t r = 0; r < original.map.grid.rows(); ++r)
        for (std::size_t c = 0; c < original.map.grid.columns(); ++c)
            assert(loaded.map.grid.at({r, c}).index() == original.map.grid.at({r, c}).index());
}

// La reproduccion de lo guardado termina exactamente igual que la partida.
void replayReproducesTheGame() {
    const Map map = sampleMap();
    const GameRecord original = playAndRecord(map);

    NavigationEnvironment live(to_scenario(map));
    const auto guard = make_guard_controller(live.grid());
    for (Action a : original.actions) (void)play_turn(live, a, guard.get());

    const GameRecord loaded = deserialize_record(serialize_record(original));
    ReplaySession session(loaded.map, loaded.actions);
    while (session.step()) {}
    const NavigationEnvironment& env = session.environment();
    assert(session.played() == original.actions.size());
    assert(env.endReason() == original.result);
    assert(env.turn() == original.turns);
    assert(env.energy() == original.final_energy);
    assert(env.player_position() == live.player_position());
    assert(env.guard_position() == live.guard_position());
    assert(env.is_switch_active(SwitchId::a) == live.is_switch_active(SwitchId::a));
    assert(env.is_switch_active(SwitchId::b) == live.is_switch_active(SwitchId::b));
}

void saveListAndLoad() {
    TempDir dir("saves");
    GameRecord first = playAndRecord(sampleMap());
    first.date = "2026-11-23 18:40:12";
    GameRecord second = first;
    second.date = "2026-11-24 09:05:00";
    const fs::path p1 = save_record(first, dir.path);   // crea la carpeta
    const fs::path p2 = save_record(second, dir.path);
    const fs::path p3 = save_record(first, dir.path);   // mismo segundo: sufijo
    assert(p1.filename() == "partida_20261123_184012.txt");
    assert(p2.filename() == "partida_20261124_090500.txt");
    assert(p3.filename() == "partida_20261123_184012_2.txt");

    const auto games = list_saved_games(dir.path);
    assert(games.size() == 3);
    assert(games[0].path == p2);  // la mas reciente primero
    for (const SavedGameEntry& g : games) {
        assert(g.record && g.error.empty());
        assert(g.summary().find("turnos") != std::string::npos);
    }
    assert(load_record(p2).date == "2026-11-24 09:05:00");
}

void invalidFilesAreReported() {
    TempDir dir("saves_bad");
    fs::create_directories(dir.path);
    std::ofstream(dir.path / "partida_20260101_000000.txt") << "resultado: victoria\n";
    std::ofstream(dir.path / "otra_cosa.txt") << "no es una partida\n";
    const auto games = list_saved_games(dir.path);
    assert(games.size() == 1);  // solo cuentan los partida_*.txt
    assert(!games[0].record && games[0].error.find("archivo invalido") != std::string::npos);
    assert(list_saved_games(dir.path / "no_existe").empty());

    const auto throws = [](const std::string& text) {
        try {
            (void)deserialize_record(text);
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };
    assert(throws("acciones: RRX\nmapa:\ngrid:\n@.S\n"));     // accion invalida
    assert(throws("resultado: gano\nacciones: R\nmapa:\n"));  // resultado desconocido
    assert(throws("acciones: R\n"));                          // falta el mapa
}

void dateFormat() {
    const std::string d = current_date();
    assert(d.size() == 19 && d[4] == '-' && d[10] == ' ' && d[13] == ':');
}

}  // namespace

int main() {
    roundTrip();
    replayReproducesTheGame();
    saveListAndLoad();
    invalidFilesAreReported();
    dateFormat();
    return 0;
}
