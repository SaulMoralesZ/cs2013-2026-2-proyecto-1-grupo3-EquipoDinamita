#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../cells.hpp"
#include "../game_rules.hpp"
#include "../grid_runtime.hpp"
#include "../position.hpp"
#include "../map_generation/map.hpp"

namespace circuit_escape {

// Scenario shim: coincide con la API prometida por Treicy (Bloque 1).
// Cuando Treicy entregue su Scenario real, este se reemplaza.
struct Scenario {
    RuntimeGrid grid{1, 1};
    Position start{};
    Position switch_a{};
    Position switch_b{};
    Position guard_start{};
    GameRules rules{};
};

// Convierte un Map (de Miguel) a un Scenario (de Treicy).
// Copia campos: grid, start, switch_a, switch_b, guard_start (vigilanteStart),
// rules. La salida no es campo: el motor la busca como la unica celda Exit.
Scenario to_scenario(const Map& map);

// Conversion Action <-> char para winning_play.
// U = up, D = down, L = left, R = right, W = wait. Siempre mayuscula.
// char_to_action lanza std::invalid_argument si el caracter no es U/D/L/R/W.
char action_to_char(Action a);
Action char_to_action(char c);

// Serializacion/deserializacion del formato de escenario.
// Formato de cuadricula: simbolos del proyecto 1 (# . ~ R B T S @).
std::string serialize(const Map& map);
Map deserialize(const std::string& text);

// Guardar/cargar escenario en archivo.
void saveMap(const Map& map, const std::string& path);
Map loadMap(const std::string& path);

}  // namespace circuit_escape