#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

#include "circuit_escape/map_generation/map_batch_runner.hpp"
#include "circuit_escape/map_io/scenario_format.hpp"
#include "circuit_escape/replay_session.hpp"
#include "circuit_escape/ui/level_catalog.hpp"

using namespace circuit_escape;
namespace fs = std::filesystem;

namespace {

// Carpeta temporal propia de la prueba (se borra al terminar).
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / ("circuit_escape_" + name)) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

// Prepara un lote real y guarda sus tres niveles, como map_generator.
void saveBatch(const fs::path& dir) {
    BatchConfig config;
    config.workers = 4;
    config.map.numResources = 3;
    config.map.numBatteries = 2;
    config.map.numTraps = 2;
    const BatchResult batch = run_map_batch(config);
    assert(batch.selection.ok());
    for (const auto& level : batch.levels) {
        saveMap(*level, (dir / level_file_name(level->level)).string());
    }
}

void catalogOfPreparedBatch() {
    TempDir dir("catalog_ok");
    saveBatch(dir.path);
    const auto catalog = load_level_catalog(dir.path);
    assert(catalog.size() == 3);
    assert(catalog[0].level == Difficulty::easy && catalog[0].label == "Facil");
    assert(catalog[1].level == Difficulty::standard && catalog[1].label == "Medio");
    assert(catalog[2].level == Difficulty::hard && catalog[2].label == "Dificil");
    for (const LevelEntry& entry : catalog) {
        assert(entry.available() && entry.error.empty());
        assert(entry.has_winning_play());
        assert(entry.map->level == entry.level);
        assert(entry.summary().find("costo minimo") != std::string::npos);
    }
    // Facil <= medio <= dificil en costo de energia.
    assert(catalog[0].map->minCost < catalog[1].map->minCost);
    assert(catalog[1].map->minCost < catalog[2].map->minCost);
}

// Cada nivel guardado se reproduce con el motor y gana; restart() reinicia.
void savedWinsReplayAndRestart() {
    TempDir dir("catalog_replay");
    saveBatch(dir.path);
    for (const LevelEntry& entry : load_level_catalog(dir.path)) {
        ReplaySession session(*entry.map);
        assert(session.played() == 0 && !session.finished());
        const Action first = session.next_action();
        while (session.step()) {}
        assert(session.played() == session.total());
        assert(session.environment().endReason() == EndReason::goalReached);
        const std::size_t turns = session.environment().turn();
        const int energy = session.environment().energy();

        // reset restaura el estado inicial: A y B desactivados, vigilante en su inicio.
        session.restart();
        const NavigationEnvironment& env = session.environment();
        assert(session.played() == 0 && session.last_events().empty());
        assert(env.turn() == 0 && !env.isFinished());
        assert(env.energy() == entry.map->rules.initialEnergy);
        assert(env.player_position() == entry.map->start);
        assert(env.guard_position() == entry.map->vigilanteStart);
        assert(!env.is_switch_active(SwitchId::a) && !env.is_switch_active(SwitchId::b));
        assert(session.next_action() == first);

        // La misma victoria se repite exactamente.
        while (session.step()) {}
        assert(env.endReason() == EndReason::goalReached);
        assert(env.turn() == turns && env.energy() == energy);
        assert(!session.step());  // nada mas que jugar
    }
}

void missingAndInvalidFiles() {
    TempDir dir("catalog_bad");
    {
        const auto catalog = load_level_catalog(dir.path / "no_existe");
        assert(catalog.size() == 3);
        for (const LevelEntry& e : catalog) assert(!e.available() && !e.error.empty());
    }
    {
        std::ofstream(dir.path / "mapa_medio.txt") << "rows: 3\ngrid:\n#?#\n";
        const auto catalog = load_level_catalog(dir.path);
        assert(!catalog[1].available());
        assert(catalog[1].error.find("archivo invalido") != std::string::npos);
        assert(catalog[0].error.find("map_generator") != std::string::npos);
    }
}

void menuCursorWraps() {
    MenuCursor cursor(4);
    assert(cursor.selected() == 0);
    cursor.up();
    assert(cursor.selected() == 3);
    cursor.down();
    cursor.down();
    assert(cursor.selected() == 1);
    cursor.select(2);
    assert(cursor.selected() == 2);
    cursor.select(9);  // fuera de rango: se ignora
    assert(cursor.selected() == 2);
    MenuCursor empty(0);
    empty.down();
    assert(empty.selected() == 0);
}

}  // namespace

int main() {
    catalogOfPreparedBatch();
    savedWinsReplayAndRestart();
    missingAndInvalidFiles();
    menuCursorWraps();
    return 0;
}
