#include "../../include/circuit_escape/map_io/scenario_format.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace circuit_escape {

Scenario to_scenario(const Map& map) {
    return Scenario{map.grid, map.start, map.switchA, map.switchB, map.vigilanteStart,
                    map.rules};
}

char action_to_char(Action a) {
    switch (a) {
        case Action::up:    return 'U';
        case Action::down:  return 'D';
        case Action::left:  return 'L';
        case Action::right: return 'R';
        case Action::wait:  return 'W';
    }
    return '?';
}

Action char_to_action(char c) {
    switch (c) {
        case 'U': return Action::up;
        case 'D': return Action::down;
        case 'L': return Action::left;
        case 'R': return Action::right;
        case 'W': return Action::wait;
        default:
            throw std::invalid_argument(
                std::string("Caracter de accion invalido: '") + c + "'");
    }
}

namespace {

// Simbolos del Proyecto 1 (ver cellFromChar en cells.hpp).
char cell_to_char(const Cell& cell) {
    return std::visit(
        Overloaded{
            [](const Wall&) { return '#'; },
            [](const Empty&) { return '.'; },
            [](const RoughTerrain&) { return '~'; },
            [](const ResourceCell<int>&) { return 'R'; },
            [](const Battery&) { return 'B'; },
            [](const Trap&) { return 'T'; },
            [](const Exit&) { return 'S'; }},
        cell);
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

Position parse_position(const std::string& key, const std::string& value) {
    std::istringstream ss(value);
    Position p{};
    if (!(ss >> p.row >> p.column)) {
        throw std::invalid_argument("Escenario: '" + key + "' espera 'fila columna'");
    }
    return p;
}

Difficulty parse_level(const std::string& value) {
    if (const auto level = parseDifficulty(value)) return *level;
    throw std::invalid_argument("Escenario: nivel desconocido '" + value + "'");
}

}  // namespace

// Formato documentado en el README (seccion "Formato de escenario").
std::string serialize(const Map& map) {
    std::ostringstream out;
    const GameRules& r = map.rules;

    out << "rows: " << map.grid.rows() << "\n";
    out << "cols: " << map.grid.columns() << "\n";
    out << "id: " << map.id << "\n";
    out << "seed: " << map.seed << "\n";
    out << "level: " << toString(map.level) << "\n";
    out << "min_cost: " << map.minCost << "\n";
    out << "energy_initial: " << r.initialEnergy << "\n";
    out << "energy_max: " << r.maximumEnergy << "\n";
    out << "turn_limit: " << r.turnLimit << "\n";
    out << "base_cost: " << r.baseCost << "\n";
    out << "rough_cost: " << r.roughCost << "\n";
    out << "wait_cost: " << r.waitCost << "\n";
    out << "invalid_cost: " << r.invalidCost << "\n";
    out << "resource_points: " << r.resourcePoints << "\n";
    out << "battery_recharge: " << r.batteryRecharge << "\n";
    out << "trap_energy_penalty: " << r.trapEnergyPenalty << "\n";
    out << "trap_score_penalty: " << r.trapScorePenalty << "\n";
    out << "start: " << map.start.row << " " << map.start.column << "\n";
    out << "switch_a: " << map.switchA.row << " " << map.switchA.column << "\n";
    out << "switch_b: " << map.switchB.row << " " << map.switchB.column << "\n";
    out << "exit: " << map.exit.row << " " << map.exit.column << "\n";
    out << "vigilante: " << map.vigilanteStart.row << " " << map.vigilanteStart.column << "\n";

    std::string wp;
    for (Action a : map.winning_play) wp += action_to_char(a);
    out << "winning_play: " << wp << "\n";

    out << "grid:\n";
    for (std::size_t row = 0; row < map.grid.rows(); ++row) {
        for (std::size_t col = 0; col < map.grid.columns(); ++col) {
            const Position pos{row, col};
            char sym = cell_to_char(map.grid.at(pos));
            if (pos == map.start && sym == '.') sym = '@';
            out << sym;
        }
        out << "\n";
    }
    return out.str();
}

Map deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    Map map;
    GameRules& r = map.rules;
    std::optional<std::size_t> rows;
    std::optional<std::size_t> cols;
    bool inGrid = false;
    std::vector<std::string> gridLines;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (inGrid) {
            if (!line.empty()) gridLines.push_back(line);
            continue;
        }
        if (trim(line) == "grid:") {
            inGrid = true;
            continue;
        }
        if (trim(line).empty() || line.front() == '#') continue;  // comentarios

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            throw std::invalid_argument("Escenario: linea sin ':' -> " + line);
        }
        const std::string key = trim(line.substr(0, colon));
        const std::string val = trim(line.substr(colon + 1));

        if (key == "rows") rows = std::stoull(val);
        else if (key == "cols") cols = std::stoull(val);
        else if (key == "id") map.id = std::stoi(val);
        else if (key == "seed") map.seed = static_cast<unsigned>(std::stoul(val));
        else if (key == "level") map.level = parse_level(val);
        else if (key == "min_cost") map.minCost = std::stoi(val);
        else if (key == "energy_initial") r.initialEnergy = std::stoi(val);
        else if (key == "energy_max") r.maximumEnergy = std::stoi(val);
        else if (key == "turn_limit") r.turnLimit = std::stoull(val);
        else if (key == "base_cost") r.baseCost = std::stoi(val);
        else if (key == "rough_cost") r.roughCost = std::stoi(val);
        else if (key == "wait_cost") r.waitCost = std::stoi(val);
        else if (key == "invalid_cost") r.invalidCost = std::stoi(val);
        else if (key == "resource_points") r.resourcePoints = std::stoi(val);
        else if (key == "battery_recharge") r.batteryRecharge = std::stoi(val);
        else if (key == "trap_energy_penalty") r.trapEnergyPenalty = std::stoi(val);
        else if (key == "trap_score_penalty") r.trapScorePenalty = std::stoi(val);
        else if (key == "start") map.start = parse_position(key, val);
        else if (key == "switch_a") map.switchA = parse_position(key, val);
        else if (key == "switch_b") map.switchB = parse_position(key, val);
        else if (key == "exit") map.exit = parse_position(key, val);
        else if (key == "vigilante") map.vigilanteStart = parse_position(key, val);
        else if (key == "winning_play") {
            map.winning_play.clear();
            for (char ch : val) {
                if (ch == ' ') continue;
                map.winning_play.push_back(char_to_action(ch));
            }
        } else {
            throw std::invalid_argument("Escenario: clave desconocida '" + key + "'");
        }
    }

    if (gridLines.empty()) throw std::invalid_argument("Escenario: falta la seccion 'grid:'");
    const std::size_t gridRows = gridLines.size();
    const std::size_t gridCols = gridLines.front().size();
    if ((rows && *rows != gridRows) || (cols && *cols != gridCols)) {
        throw std::invalid_argument("Escenario: rows/cols no coinciden con la cuadricula (" +
                                    std::to_string(gridRows) + "x" +
                                    std::to_string(gridCols) + ")");
    }

    map.grid = Grid<Cell>(gridRows, gridCols);  // valida dimensiones
    for (std::size_t row = 0; row < gridRows; ++row) {
        if (gridLines[row].size() != gridCols) {
            throw std::invalid_argument("Escenario: la fila " + std::to_string(row) +
                                        " de la cuadricula tiene otro ancho");
        }
        for (std::size_t col = 0; col < gridCols; ++col) {
            const auto cell = cellFromChar(gridLines[row][col]);
            if (!cell) {
                throw std::invalid_argument(std::string("Escenario: simbolo desconocido '") +
                                            gridLines[row][col] + "' en " +
                                            toString(Position{row, col}));
            }
            map.grid.at({row, col}) = *cell;
        }
    }

    // Copias planas que conserva Map por compatibilidad.
    map.energyInitial = r.initialEnergy;
    map.energyMax = r.maximumEnergy;
    map.turnLimit = r.turnLimit;
    return map;
}

void saveMap(const Map& map, const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("No se pudo abrir el archivo para guardar: " + path);
    }
    file << serialize(map);
}

Map loadMap(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("No se pudo abrir el archivo para cargar: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return deserialize(ss.str());
}

}  // namespace circuit_escape
