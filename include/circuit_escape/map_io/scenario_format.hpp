#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../cells.hpp"
#include "../game_rules.hpp"
#include "../grid.hpp"
#include "../position.hpp"
#include "../map_generation/map.hpp"
#include "../scenario.hpp"

namespace circuit_escape {

// Convierte un Map (generado o cargado) al Scenario que consume el motor.
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