#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../cells.hpp"
#include "../game_rules.hpp"
#include "../grid.hpp"
#include "../position.hpp"

namespace circuit_escape {

// Configuracion para construir un mapa (energia, costos, limites, etc.)
struct MapConfig {
    int energyInitial{60};
    int energyMax{60};
    std::size_t turnLimit{180};
    int baseCost{1};
    int roughCost{2};
    int waitCost{1};
    int invalidCost{1};
    int resourcePoints{10};
    int batteryRecharge{3};
    int trapEnergyPenalty{2};
    int trapScorePenalty{1};
    int numResources{0};
    int numBatteries{0};
    int numTraps{0};
};

// Struct de escenario (proveido a los demas bloques).
// Campos: id, seed, grid, start, switchA, switchB, exit, vigilanteStart,
//         energyInitial, energyMax, turnLimit, rules, minCost, level, winning_play.
struct Map {
    int id{0};
    unsigned seed{0};
    Grid<Cell> grid{1, 1};  // placeholder, se reemplaza en buildMap
    Position start{};
    Position switchA{};
    Position switchB{};
    Position exit{};
    Position vigilanteStart{};
    int energyInitial{60};
    int energyMax{60};
    std::size_t turnLimit{180};
    GameRules rules{};
    int minCost{0};
    Difficulty level{Difficulty::standard};
    std::vector<Action> winning_play{};
    // Kruskal (seccion 2.1): muros retirados por el arbol y, por separado,
    // las aperturas adicionales que crean caminos alternativos.
    std::vector<Position> tree_openings{};
    std::vector<Position> extra_openings{};
    long long tree_cost{0};  // suma de los valores aleatorios del arbol

};

}  // namespace circuit_escape