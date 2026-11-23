#undef NDEBUG
#include <cassert>
#include <optional>
#include <unordered_set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/circuit_escape/cells.hpp"
#include "../include/circuit_escape/game_rules.hpp"
#include "../include/circuit_escape/grid_runtime.hpp"
#include "../include/circuit_escape/map_generation/map.hpp"
#include "../include/circuit_escape/map_generation/map_builder.hpp"
#include "../include/circuit_escape/map_io/scenario_format.hpp"
#include "../include/circuit_escape/position.hpp"

using namespace circuit_escape;

namespace {

void validateDimensionsBasic() {
    // Invalidas
    assert(MapBuilder::validateDimensions(0, 5).has_value());
    assert(MapBuilder::validateDimensions(5, 0).has_value());
    assert(MapBuilder::validateDimensions(-1, 5).has_value());
    assert(MapBuilder::validateDimensions(5, -1).has_value());
    // Validas
    assert(!MapBuilder::validateDimensions(10, 15).has_value());
    assert(!MapBuilder::validateDimensions(20, 30).has_value());
    assert(!MapBuilder::validateDimensions(40, 60).has_value());
    // Borde: max_side y max_side+1
    assert(!MapBuilder::validateDimensions(
        static_cast<int>(RuntimeGrid::max_side),
        static_cast<int>(RuntimeGrid::max_side)).has_value());
    assert(MapBuilder::validateDimensions(
        static_cast<int>(RuntimeGrid::max_side) + 1,
        5).has_value());
    // Demasiado pequenas
    assert(MapBuilder::validateDimensions(1, 1).has_value());
    assert(MapBuilder::validateDimensions(1, 4).has_value());
}

void buildMapBasic() {
    MapConfig config;
    config.energyInitial = 60;
    config.energyMax = 60;
    config.turnLimit = 180;
    config.baseCost = 1;
    config.roughCost = 2;

    Map map = MapBuilder::buildMap(1, 42, 10, 15, config);

    assert(map.id == 1);
    assert(map.seed == 42);
    assert(map.grid.rows() == 10);
    assert(map.grid.columns() == 15);
    assert(map.energyInitial == 60);
    assert(map.energyMax == 60);
    assert(map.turnLimit == 180);

    // Invariante: una sola celda Exit
    int exitCount = 0;
    for (std::size_t r = 0; r < map.grid.rows(); ++r)
        for (std::size_t c = 0; c < map.grid.columns(); ++c)
            if (std::holds_alternative<Exit>(map.grid.at({r, c}))) ++exitCount;
    assert(exitCount == 1);

    // Invariante: start, switchA, switchB, vigilanteStart en celdas transitables y distintas
    assert(isTraversable(map.grid.at(map.start)));
    assert(isTraversable(map.grid.at(map.switchA)));
    assert(isTraversable(map.grid.at(map.switchB)));
    assert(isTraversable(map.grid.at(map.vigilanteStart)));
    // No sobre Exit
    assert(!std::holds_alternative<Exit>(map.grid.at(map.start)));
    assert(!std::holds_alternative<Exit>(map.grid.at(map.switchA)));
    assert(!std::holds_alternative<Exit>(map.grid.at(map.switchB)));
    assert(!std::holds_alternative<Exit>(map.grid.at(map.vigilanteStart)));
    // Distintas
    std::unordered_set<Position, PositionHash> positions = {map.start, map.switchA, map.switchB, map.vigilanteStart, map.exit};
    assert(positions.size() == 5);

    // rules valido
    assert(map.rules.initialEnergy > 0);
    assert(map.rules.maximumEnergy >= map.rules.initialEnergy);
    assert(map.rules.turnLimit > 0);
}

void buildMapReproducible() {
    MapConfig config;
    Map m1 = MapBuilder::buildMap(1, 42, 10, 15, config);
    Map m2 = MapBuilder::buildMap(1, 42, 10, 15, config);

    // Misma semilla => mismo grid
    for (std::size_t r = 0; r < m1.grid.rows(); ++r)
        for (std::size_t c = 0; c < m1.grid.columns(); ++c) {
            // Comparar tipo de celda
            assert(m1.grid.at({r, c}).index() == m2.grid.at({r, c}).index());
        }
    assert(m1.start == m2.start);
    assert(m1.switchA == m2.switchA);
    assert(m1.switchB == m2.switchB);
    assert(m1.exit == m2.exit);
    assert(m1.vigilanteStart == m2.vigilanteStart);
}

void buildMapDifferentSeeds() {
    MapConfig config;
    Map m1 = MapBuilder::buildMap(1, 42, 10, 15, config);
    Map m2 = MapBuilder::buildMap(1, 99, 10, 15, config);

    // Diferente semilla => diferente grid (al menos una celda diferente)
    bool different = false;
    for (std::size_t r = 0; r < m1.grid.rows() && !different; ++r)
        for (std::size_t c = 0; c < m1.grid.columns() && !different; ++c) {
            if (m1.grid.at({r, c}).index() != m2.grid.at({r, c}).index()) {
                different = true;
            }
        }
    assert(different);
}

void buildMapThrowsInvalid() {
    MapConfig config;
    bool threw = false;
    try {
        MapBuilder::buildMap(1, 42, 0, 15, config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        MapBuilder::buildMap(1, 42, 10, 0, config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void actionConversion() {
    // Round-trip
    for (Action a : {Action::up, Action::down, Action::left, Action::right, Action::wait}) {
        char ch = action_to_char(a);
        Action back = char_to_action(ch);
        assert(back == a);
    }
    // Valores esperados
    assert(action_to_char(Action::up) == 'U');
    assert(action_to_char(Action::down) == 'D');
    assert(action_to_char(Action::left) == 'L');
    assert(action_to_char(Action::right) == 'R');
    assert(action_to_char(Action::wait) == 'W');

    // Caracter invalido lanza
    bool threw = false;
    try {
        char_to_action('X');
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    // minuscula lanza
    threw = false;
    try {
        char_to_action('u');
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void winningPlaySerialization() {
    MapConfig config;
    Map map = MapBuilder::buildMap(1, 42, 10, 15, config);
    map.winning_play = {Action::up, Action::up, Action::right, Action::down, Action::down, Action::wait};

    std::string text = serialize(map);
    // Verificar que winning_play esta en el texto
    assert(text.find("winning_play: UURDDW") != std::string::npos);

    // Deserializar y verificar
    Map loaded = deserialize(text);
    assert(loaded.winning_play.size() == 6);
    assert(loaded.winning_play[0] == Action::up);
    assert(loaded.winning_play[1] == Action::up);
    assert(loaded.winning_play[2] == Action::right);
    assert(loaded.winning_play[3] == Action::down);
    assert(loaded.winning_play[4] == Action::down);
    assert(loaded.winning_play[5] == Action::wait);
}

void toScenario() {
    MapConfig config;
    Map map = MapBuilder::buildMap(1, 42, 10, 15, config);
    Scenario s = to_scenario(map);
    assert(s.grid.rows() == map.grid.rows());
    assert(s.grid.columns() == map.grid.columns());
    assert(s.start == map.start);
    assert(s.switch_a == map.switchA);
    assert(s.switch_b == map.switchB);
    assert(s.guard_start == map.vigilanteStart);
    assert(s.rules.initialEnergy == map.rules.initialEnergy);
}

void saveLoadRoundTrip() {
    MapConfig config;
    Map map = MapBuilder::buildMap(1, 42, 10, 15, config);
    map.winning_play = {Action::up, Action::right};

    const std::string path = "test_map_roundtrip.txt";
    saveMap(map, path);
    Map loaded = loadMap(path);

    assert(loaded.id == map.id);
    assert(loaded.seed == map.seed);
    assert(loaded.grid.rows() == map.grid.rows());
    assert(loaded.grid.columns() == map.grid.columns());
    assert(loaded.start == map.start);
    assert(loaded.switchA == map.switchA);
    assert(loaded.switchB == map.switchB);
    assert(loaded.exit == map.exit);
    assert(loaded.vigilanteStart == map.vigilanteStart);
    assert(loaded.winning_play.size() == 2);
    assert(loaded.winning_play[0] == Action::up);
    assert(loaded.winning_play[1] == Action::right);

    // Limpiar
    std::remove(path.c_str());
}

}  // namespace

int main() {
    validateDimensionsBasic();
    buildMapBasic();
    buildMapReproducible();
    buildMapDifferentSeeds();
    buildMapThrowsInvalid();
    actionConversion();
    winningPlaySerialization();
    toScenario();
    saveLoadRoundTrip();
    return 0;
}