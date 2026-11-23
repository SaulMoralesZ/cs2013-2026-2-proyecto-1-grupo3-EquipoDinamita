#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "../cells.hpp"
#include "../game_rules.hpp"
#include "../grid_runtime.hpp"
#include "../position.hpp"

#include "map.hpp"
#include "../graph/disjoint_set.hpp"
#include "../graph/graph.hpp"

namespace circuit_escape {

class MapBuilder {
public:
    // Valida dimensiones: devuelve mensaje de error si invalidas, nullopt si OK.
    // Reutiliza max_side de Grid. Rechaza nulas/negativas, sin espacio para 5
    // elementos, que excedan recursos. 40x60 debe pasar.
    static std::optional<std::string> validateDimensions(int rows, int cols);

    // Construye un mapa completo con Kruskal.
    static Map buildMap(int mapId, unsigned seed, int rows, int cols, const MapConfig& config);
};

}  // namespace circuit_escape