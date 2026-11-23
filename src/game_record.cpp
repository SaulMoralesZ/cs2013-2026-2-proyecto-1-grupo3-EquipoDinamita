#include "../include/circuit_escape/game_record.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "../include/circuit_escape/map_io/scenario_format.hpp"
#include "../include/circuit_escape/ui/level_catalog.hpp"

namespace circuit_escape {

namespace {

std::string result_code(EndReason reason) {
    switch (reason) {
        case EndReason::goalReached: return "victoria";
        case EndReason::noEnergy: return "sin_energia";
        case EndReason::turnLimit: return "limite_turnos";
        case EndReason::captured: return "capturado";
        case EndReason::none: return "en_curso";
    }
    return "en_curso";
}

EndReason parse_result(const std::string& code) {
    if (code == "victoria") return EndReason::goalReached;
    if (code == "sin_energia") return EndReason::noEnergy;
    if (code == "limite_turnos") return EndReason::turnLimit;
    if (code == "capturado") return EndReason::captured;
    if (code == "en_curso") return EndReason::none;
    throw std::invalid_argument("Partida: resultado desconocido '" + code + "'");
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}

}  // namespace

std::string current_date() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string serialize_record(const GameRecord& record) {
    std::ostringstream out;
    out << "resultado: " << result_code(record.result) << "\n";
    out << "turnos: " << record.turns << "\n";
    out << "energia_final: " << record.final_energy << "\n";
    out << "fecha: " << record.date << "\n";
    std::string actions;
    for (Action a : record.actions) actions += action_to_char(a);
    out << "acciones: " << actions << "\n";
    out << "mapa:\n" << serialize(record.map);
    return out.str();
}

GameRecord deserialize_record(const std::string& text) {
    GameRecord record;
    std::istringstream in(text);
    std::string line;
    bool hasMap = false;
    bool hasActions = false;
    while (std::getline(in, line)) {
        if (trim(line) == "mapa:") {
            hasMap = true;
            break;
        }
        if (trim(line).empty()) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            throw std::invalid_argument("Partida: linea sin ':' -> " + line);
        }
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (key == "resultado") record.result = parse_result(value);
        else if (key == "turnos") record.turns = std::stoull(value);
        else if (key == "energia_final") record.final_energy = std::stoi(value);
        else if (key == "fecha") record.date = value;
        else if (key == "acciones") {
            hasActions = true;
            for (char ch : value) record.actions.push_back(char_to_action(ch));
        } else {
            throw std::invalid_argument("Partida: clave desconocida '" + key + "'");
        }
    }
    if (!hasActions) throw std::invalid_argument("Partida: faltan las acciones");
    if (!hasMap) throw std::invalid_argument("Partida: falta la seccion 'mapa:'");
    std::ostringstream rest;
    rest << in.rdbuf();
    record.map = deserialize(rest.str());
    return record;
}

std::filesystem::path save_record(GameRecord record, const std::filesystem::path& directory) {
    if (record.date.empty()) record.date = current_date();
    std::filesystem::create_directories(directory);

    // partida_AAAAMMDD_HHMMSS.txt; si ya existe (mismo segundo), sufijo _2, _3...
    std::string compact;
    for (char ch : record.date) {
        if (ch >= '0' && ch <= '9') compact += ch;
        else if (ch == ' ') compact += '_';
    }
    std::filesystem::path path = directory / ("partida_" + compact + ".txt");
    for (int n = 2; std::filesystem::exists(path); ++n) {
        path = directory / ("partida_" + compact + "_" + std::to_string(n) + ".txt");
    }

    std::ofstream file(path);
    if (!file) throw std::runtime_error("No se pudo guardar la partida en " + path.string());
    file << serialize_record(record);
    return path;
}

GameRecord load_record(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("No se pudo abrir la partida: " + path.string());
    std::ostringstream text;
    text << file.rdbuf();
    return deserialize_record(text.str());
}

std::string SavedGameEntry::summary() const {
    if (!record) return error;
    const GameRecord& r = *record;
    return toString(r.result) + " en " + std::to_string(r.turns) + " turnos, energia " +
           std::to_string(r.final_energy) + " | " + level_label(r.map.level) + " | mapa " +
           std::to_string(r.map.id) + " " + std::to_string(r.map.grid.rows()) + "x" +
           std::to_string(r.map.grid.columns());
}

std::vector<SavedGameEntry> list_saved_games(const std::filesystem::path& directory) {
    std::vector<SavedGameEntry> games;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return games;
    for (const auto& file : std::filesystem::directory_iterator(directory, ec)) {
        const auto name = file.path().filename().string();
        if (!file.is_regular_file() || name.rfind("partida_", 0) != 0 ||
            file.path().extension() != ".txt") {
            continue;
        }
        SavedGameEntry entry;
        entry.path = file.path();
        try {
            entry.record = load_record(file.path());
        } catch (const std::exception& error) {
            entry.error = std::string("archivo invalido: ") + error.what();
        }
        games.push_back(std::move(entry));
    }
    // El nombre lleva la fecha: orden descendente = mas reciente primero.
    std::sort(games.begin(), games.end(), [](const SavedGameEntry& a, const SavedGameEntry& b) {
        return a.path.filename().string() > b.path.filename().string();
    });
    return games;
}

}  // namespace circuit_escape
