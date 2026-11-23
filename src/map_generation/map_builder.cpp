#include "../../include/circuit_escape/map_generation/map_builder.hpp"

#include <algorithm>
#include <random>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace circuit_escape {

namespace {

// Casillas transitables que deja Kruskal en una cuadricula rows x cols:
// las casillas base (filas y columnas pares) mas las aperturas del arbol.
std::size_t tree_traversable_cells(std::size_t rows, std::size_t cols) {
    const std::size_t base = ((rows + 1) / 2) * ((cols + 1) / 2);
    return base == 0 ? 0 : 2 * base - 1;
}

// Fisher-Yates propio: std::shuffle depende de la implementacion de la
// biblioteca estandar y la misma semilla daria mapas distintos segun el
// compilador. La salida de std::mt19937 si esta fijada por el estandar.
template<typename T>
void portable_shuffle(std::vector<T>& values, std::mt19937& rng) {
    for (std::size_t i = values.size(); i > 1; --i) {
        const std::size_t j = static_cast<std::size_t>(rng() % i);
        std::swap(values[i - 1], values[j]);
    }
}

}  // namespace

std::optional<std::string> MapBuilder::validateDimensions(int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        return "Dimensiones invalidas: filas y columnas deben ser positivas.";
    }
    // Reutilizar max_side de Grid (Treicy), usar por nombre.
    if (static_cast<std::size_t>(rows) > Grid<Cell>::max_side ||
        static_cast<std::size_t>(cols) > Grid<Cell>::max_side) {
        return "Dimensiones exceden el maximo permitido (" +
               std::to_string(Grid<Cell>::max_side) + ").";
    }
    // Necesitamos al menos espacio para 5 elementos (inicio, A, B, salida, vigilante)
    // en casillas transitables. Una grilla muy pequena no puede contenerlos.
    if (tree_traversable_cells(static_cast<std::size_t>(rows),
                               static_cast<std::size_t>(cols)) < 5) {
        return "Dimensiones demasiado pequenas: se necesitan al menos 5 casillas transitables.";
    }
    return std::nullopt;
}

Map MapBuilder::buildMap(int mapId, unsigned seed, int rows, int cols,
                         const MapConfig& config) {
    // Validar dimensiones
    if (auto err = validateDimensions(rows, cols)) {
        throw std::invalid_argument(*err);
    }

    Map map;
    map.id = mapId;
    map.seed = seed;

    // Crear grid de dimensiones dadas, todo muros.
    Grid<Cell> grid(static_cast<std::size_t>(rows), static_cast<std::size_t>(cols));
    for (std::size_t r = 0; r < grid.rows(); ++r) {
        for (std::size_t c = 0; c < grid.columns(); ++c) {
            grid.at({r, c}) = Wall{};
        }
    }

    // Paso 1: Preparar casillas libres separadas por muros.
    // Casillas libres en posiciones (r % 2 == 0 && c % 2 == 0).
    std::vector<Position> freeCells;
    for (std::size_t r = 0; r < grid.rows(); r += 2) {
        for (std::size_t c = 0; c < grid.columns(); c += 2) {
            grid.at({r, c}) = Empty{};
            freeCells.push_back({r, c});
        }
    }

    // Si no hay casillas libres, no se puede generar.
    if (freeCells.empty()) {
        throw std::invalid_argument("No se pueden generar casillas libres con estas dimensiones.");
    }

    // Paso 2: Identificar muros removibles (entre dos casillas libres adyacentes).
    // Un muro en (r, c) es removable si es Wall y tiene casillas libres en ambos lados.
    struct Opening {
        Position wall;
        int value;
        std::size_t order;  // desempate fijo entre valores iguales
    };
    std::vector<Opening> openings;
    std::mt19937 rng(seed);

    // Muros horizontales entre casillas libres (misma fila, columnas adyacentes)
    for (std::size_t r = 0; r < grid.rows(); r += 2) {
        for (std::size_t c = 1; c + 1 < grid.columns(); c += 2) {
            Position left{r, c - 1};
            Position right{r, c + 1};
            Position wall{r, c};
            if (grid.contains(left) && grid.contains(right) &&
                std::holds_alternative<Empty>(grid.at(left)) &&
                std::holds_alternative<Empty>(grid.at(right)) &&
                std::holds_alternative<Wall>(grid.at(wall))) {
                openings.push_back({wall, static_cast<int>(rng() % 1000000u) + 1, openings.size()});
            }
        }
    }

    // Muros verticales entre casillas libres (misma columna, filas adyacentes)
    for (std::size_t c = 0; c < grid.columns(); c += 2) {
        for (std::size_t r = 1; r + 1 < grid.rows(); r += 2) {
            Position up{r - 1, c};
            Position down{r + 1, c};
            Position wall{r, c};
            if (grid.contains(up) && grid.contains(down) &&
                std::holds_alternative<Empty>(grid.at(up)) &&
                std::holds_alternative<Empty>(grid.at(down)) &&
                std::holds_alternative<Wall>(grid.at(wall))) {
                openings.push_back({wall, static_cast<int>(rng() % 1000000u) + 1, openings.size()});
            }
        }
    }

    // Paso 3: cada apertura ya tiene un valor positivo al azar (1..1000000).
    // Paso 4: Kruskal - ordenar aperturas de menor a mayor valor.
    std::sort(openings.begin(), openings.end(),
              [](const Opening& a, const Opening& b) {
                  if (a.value != b.value) return a.value < b.value;
                  return a.order < b.order;
              });

    // DisjointSet para las casillas libres.
    // Mapear cada casilla libre a un indice.
    std::unordered_map<Position, int, PositionHash> cellIndex;
    for (std::size_t i = 0; i < freeCells.size(); ++i) {
        cellIndex[freeCells[i]] = static_cast<int>(i);
    }

    DisjointSet<int> ds;
    ds.makeSet(freeCells.size());

    // Remover muros elegidos por Kruskal.
    for (const auto& opening : openings) {
        Position left = {opening.wall.row, opening.wall.column - 1};
        Position right = {opening.wall.row, opening.wall.column + 1};
        Position up = {opening.wall.row - 1, opening.wall.column};
        Position down = {opening.wall.row + 1, opening.wall.column};

        // Determinar las dos casillas que conecta este muro.
        Position cell1, cell2;
        bool horizontal = (opening.wall.row % 2 == 0);
        if (horizontal) {
            cell1 = left;
            cell2 = right;
        } else {
            cell1 = up;
            cell2 = down;
        }

        auto it1 = cellIndex.find(cell1);
        auto it2 = cellIndex.find(cell2);
        if (it1 != cellIndex.end() && it2 != cellIndex.end()) {
            if (ds.unionSets(it1->second, it2->second)) {
                // Remover el muro (convertir a Empty)
                grid.at(opening.wall) = Empty{};
                map.tree_openings.push_back(opening.wall);
                map.tree_cost += opening.value;
            }
        }
    }

    // Paso 5: Retirar algunos mas para crear caminos alternativos.
    // Remover ~10% de los muros restantes entre casillas libres.
    std::vector<Opening> remaining;
    for (const auto& o : openings) {
        if (std::holds_alternative<Wall>(grid.at(o.wall))) {
            remaining.push_back(o);
        }
    }
    portable_shuffle(remaining, rng);
    std::size_t extra = remaining.size() / 10;
    for (std::size_t i = 0; i < extra && i < remaining.size(); ++i) {
        grid.at(remaining[i].wall) = Empty{};
        map.extra_openings.push_back(remaining[i].wall);
    }

    // Colocar elementos: inicio, A, B, salida, vigilante.
    // Primero recoger todas las casillas transitables (libres).
    std::vector<Position> traversable;
    for (std::size_t r = 0; r < grid.rows(); ++r) {
        for (std::size_t c = 0; c < grid.columns(); ++c) {
            Position pos{r, c};
            if (isTraversable(grid.at(pos))) {
                traversable.push_back(pos);
            }
        }
    }

    const std::size_t needed = 5 + static_cast<std::size_t>(std::max(0, config.numResources)) +
                               static_cast<std::size_t>(std::max(0, config.numBatteries)) +
                               static_cast<std::size_t>(std::max(0, config.numTraps));
    if (traversable.size() < needed) {
        throw std::invalid_argument(
            "No hay suficientes casillas transitables para colocar los elementos (" +
            std::to_string(traversable.size()) + " disponibles, " + std::to_string(needed) +
            " necesarias).");
    }

    // Seleccionar 5 casillas distintas para inicio, A, B, salida, vigilante.
    portable_shuffle(traversable, rng);
    Position startPos = traversable[0];
    Position switchA = traversable[1];
    Position switchB = traversable[2];
    Position exitPos = traversable[3];
    Position vigilanteStart = traversable[4];

    // Colocar la salida como celda Exit.
    grid.at(exitPos) = Exit{};

    // Repartir baterias, trampas y recursos en casillas transitables
    // (evitando las 5 casillas de elementos).
    std::unordered_set<Position, PositionHash> reserved = {startPos, switchA, switchB, exitPos, vigilanteStart};

    // Recursos
    int placed = 0;
    for (Position pos : traversable) {
        if (placed >= config.numResources) break;
        if (reserved.count(pos)) continue;
        if (std::holds_alternative<Empty>(grid.at(pos))) {
            grid.at(pos) = ResourceCell<int>{config.resourcePoints, false};
            ++placed;
        }
    }

    // Baterias
    placed = 0;
    for (Position pos : traversable) {
        if (placed >= config.numBatteries) break;
        if (reserved.count(pos)) continue;
        if (std::holds_alternative<Empty>(grid.at(pos))) {
            grid.at(pos) = Battery{false};
            ++placed;
        }
    }

    // Trampas
    placed = 0;
    for (Position pos : traversable) {
        if (placed >= config.numTraps) break;
        if (reserved.count(pos)) continue;
        if (std::holds_alternative<Empty>(grid.at(pos))) {
            grid.at(pos) = Trap{};
            ++placed;
        }
    }

    // Algunos terrenos asperos en casillas libres restantes.
    for (Position pos : traversable) {
        if (reserved.count(pos)) continue;
        if (std::holds_alternative<Empty>(grid.at(pos))) {
            // ~20% de las casillas libres restantes son terreno aspero
            if (rng() % 5 == 0) {
                grid.at(pos) = RoughTerrain{};
            }
        }
    }

    // Construir GameRules desde MapConfig.
    GameRules rules;
    rules.waitCost = config.waitCost;
    rules.invalidCost = config.invalidCost;
    rules.initialEnergy = config.energyInitial;
    rules.maximumEnergy = config.energyMax;
    rules.turnLimit = config.turnLimit;
    rules.baseCost = config.baseCost;
    rules.roughCost = config.roughCost;
    rules.resourcePoints = config.resourcePoints;
    rules.batteryRecharge = config.batteryRecharge;
    rules.trapEnergyPenalty = config.trapEnergyPenalty;
    rules.trapScorePenalty = config.trapScorePenalty;

    // Llenar el Map.
    map.grid = std::move(grid);
    map.start = startPos;
    map.switchA = switchA;
    map.switchB = switchB;
    map.exit = exitPos;
    map.vigilanteStart = vigilanteStart;
    map.energyInitial = config.energyInitial;
    map.energyMax = config.energyMax;
    map.turnLimit = config.turnLimit;
    map.rules = rules;
    map.minCost = 0;  // Se calcula con Dijkstra (Bloque 3)
    map.level = Difficulty::standard;  // Se clasifica en MapSelector (Bloque 3)
    map.winning_play.clear();  // Se llena en validacion (Bloque 3/4)

    return map;
}

}  // namespace circuit_escape