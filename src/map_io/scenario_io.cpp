#include "../../include/circuit_escape/map_io/scenario_format.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace circuit_escape {

Scenario to_scenario(const Map& map) {
    Scenario s;
    s.grid = map.grid;  // copy
    s.start = map.start;
    s.switch_a = map.switchA;
    s.switch_b = map.switchB;
    s.guard_start = map.vigilanteStart;
    s.rules = map.rules;
    return s;
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

// Serializacion: formato de texto con cuadricula + metadatos.
std::string serialize(const Map& map) {
    std::ostringstream out;

    // Metadatos
    out << "rows: " << map.grid.rows() << "\n";
    out << "cols: " << map.grid.columns() << "\n";
    out << "id: " << map.id << "\n";
    out << "seed: " << map.seed << "\n";
    out << "level: " << toString(map.level) << "\n";
    out << "energy_initial: " << map.energyInitial << "\n";
    out << "energy_max: " << map.energyMax << "\n";
    out << "turn_limit: " << map.turnLimit << "\n";
    out << "base_cost: " << map.rules.baseCost << "\n";
    out << "rough_cost: " << map.rules.roughCost << "\n";
    out << "min_cost: " << map.minCost << "\n";
    out << "start: " << map.start.row << " " << map.start.column << "\n";
    out << "switch_a: " << map.switchA.row << " " << map.switchA.column << "\n";
    out << "switch_b: " << map.switchB.row << " " << map.switchB.column << "\n";
    out << "exit: " << map.exit.row << " " << map.exit.column << "\n";
    out << "vigilante: " << map.vigilanteStart.row << " " << map.vigilanteStart.column << "\n";

    // winning_play
    std::string wp;
    for (Action a : map.winning_play) {
        wp += action_to_char(a);
    }
    out << "winning_play: " << wp << "\n";

    // Cuadricula
    out << "grid:\n";
    for (std::size_t r = 0; r < map.grid.rows(); ++r) {
        for (std::size_t c = 0; c < map.grid.columns(); ++c) {
            Position pos{r, c};
            Cell cell = map.grid.at(pos);
            // Convertir Cell a char usando los simbolos del proyecto 1.
            char sym = '?';
            if (std::holds_alternative<Wall>(cell)) sym = '#';
            else if (std::holds_alternative<Empty>(cell)) sym = '.';
            else if (std::holds_alternative<RoughTerrain>(cell)) sym = '~';
            else if (std::holds_alternative<ResourceCell<int>>(cell)) sym = 'R';
            else if (std::holds_alternative<Battery>(cell)) sym = 'B';
            else if (std::holds_alternative<Trap>(cell)) sym = 'T';
            else if (std::holds_alternative<Exit>(cell)) sym = 'S';
            // Start position: if it's the start, use @
            if (pos == map.start && sym == '.') sym = '@';
            out << sym;
        }
        out << "\n";
    }

    return out.str();
}

Map deserialize(const std::string& text) {
    // Parse simple: leer metadatos y cuadricula.
    std::istringstream in(text);
    std::string line;
    Map map;
    bool inGrid = false;
    std::vector<std::string> gridLines;

    while (std::getline(in, line)) {
        if (line == "grid:") {
            inGrid = true;
            continue;
        }
        if (inGrid) {
            if (line.empty()) continue;
            gridLines.push_back(line);
            continue;
        }
        // Parse metadata
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // trim
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        while (!val.empty() && val.back() == ' ') val.pop_back();

        if (key == "rows") map.grid = RuntimeGrid(std::stoull(val), map.grid.columns() > 0 ? map.grid.columns() : 1);
        else if (key == "cols") {
            // Re-create grid with correct dimensions
            std::size_t r = map.grid.rows();
            map.grid = RuntimeGrid(r, std::stoull(val));
        }
        else if (key == "id") map.id = std::stoi(val);
        else if (key == "seed") map.seed = std::stoul(val);
        else if (key == "level") {
            if (val == "easy") map.level = Difficulty::easy;
            else if (val == "hard") map.level = Difficulty::hard;
            else map.level = Difficulty::standard;
        }
        else if (key == "energy_initial") map.energyInitial = std::stoi(val);
        else if (key == "energy_max") map.energyMax = std::stoi(val);
        else if (key == "turn_limit") map.turnLimit = std::stoull(val);
        else if (key == "base_cost") map.rules.baseCost = std::stoi(val);
        else if (key == "rough_cost") map.rules.roughCost = std::stoi(val);
        else if (key == "min_cost") map.minCost = std::stoi(val);
        else if (key == "start") {
            std::istringstream ss(val);
            ss >> map.start.row >> map.start.column;
        }
        else if (key == "switch_a") {
            std::istringstream ss(val);
            ss >> map.switchA.row >> map.switchA.column;
        }
        else if (key == "switch_b") {
            std::istringstream ss(val);
            ss >> map.switchB.row >> map.switchB.column;
        }
        else if (key == "exit") {
            std::istringstream ss(val);
            ss >> map.exit.row >> map.exit.column;
        }
        else if (key == "vigilante") {
            std::istringstream ss(val);
            ss >> map.vigilanteStart.row >> map.vigilanteStart.column;
        }
        else if (key == "winning_play") {
            map.winning_play.clear();
            for (char ch : val) {
                if (ch == ' ' || ch == '\r') continue;
                map.winning_play.push_back(char_to_action(ch));
            }
        }
    }

    // Parse grid
    if (!gridLines.empty()) {
        // Ensure grid dimensions match
        std::size_t rows = gridLines.size();
        std::size_t cols = gridLines.empty() ? 0 : gridLines[0].size();
        map.grid = RuntimeGrid(rows, cols);
        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < cols && c < gridLines[r].size(); ++c) {
                auto cell = cellFromChar(gridLines[r][c]);
                if (cell) {
                    map.grid.at({r, c}) = *cell;
                }
            }
        }
    }

    // Set rules from parsed values
    map.rules.initialEnergy = map.energyInitial;
    map.rules.maximumEnergy = map.energyMax;
    map.rules.turnLimit = map.turnLimit;

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