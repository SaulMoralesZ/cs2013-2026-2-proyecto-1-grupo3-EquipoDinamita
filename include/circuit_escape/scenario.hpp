#pragma once

#include "./cells.hpp"
#include "./game_rules.hpp"
#include "./grid.hpp"
#include "./position.hpp"

namespace circuit_escape {

// Escenario completo de una partida (Proyecto 2). La salida no es un campo:
// el motor la encuentra como la unica celda Exit del Grid.
// Lo comparten el motor (NavigationEnvironment) y map_io (to_scenario).
struct Scenario {
    Grid<Cell> grid;
    Position start{};
    Position switch_a{};
    Position switch_b{};
    Position guard_start{};
    GameRules rules{};
};

}  // namespace circuit_escape
