#include "../../include/circuit_escape/ui/level_catalog.hpp"

#include <exception>

#include "../../include/circuit_escape/environment.hpp"
#include "../../include/circuit_escape/map_io/scenario_format.hpp"

namespace circuit_escape {

std::string level_file_name(Difficulty level) {
    switch (level) {
        case Difficulty::easy: return "mapa_facil.txt";
        case Difficulty::standard: return "mapa_medio.txt";
        case Difficulty::hard: return "mapa_dificil.txt";
    }
    return "mapa_medio.txt";
}

std::string level_label(Difficulty level) {
    switch (level) {
        case Difficulty::easy: return "Facil";
        case Difficulty::standard: return "Medio";
        case Difficulty::hard: return "Dificil";
    }
    return "Medio";
}

std::string LevelEntry::summary() const {
    if (!map) return error;
    return "mapa " + std::to_string(map->id) + " | " + std::to_string(map->grid.rows()) + "x" +
           std::to_string(map->grid.columns()) + " | costo minimo " +
           std::to_string(map->minCost) + " | energia " +
           std::to_string(map->rules.initialEnergy) + " | " +
           std::to_string(map->rules.turnLimit) + " turnos";
}

std::vector<LevelEntry> load_level_catalog(const std::filesystem::path& directory) {
    std::vector<LevelEntry> catalog;
    for (Difficulty level : {Difficulty::easy, Difficulty::standard, Difficulty::hard}) {
        LevelEntry entry;
        entry.level = level;
        entry.label = level_label(level);
        entry.path = directory / level_file_name(level);
        if (!std::filesystem::exists(entry.path)) {
            entry.error = "no hay mapa: genere el lote con map_generator";
        } else {
            try {
                Map map = loadMap(entry.path.string());
                // El motor valida posiciones, salida y reglas antes de ofrecerlo.
                (void)NavigationEnvironment(to_scenario(map));
                entry.map = std::move(map);
            } catch (const std::exception& error) {
                entry.error = std::string("archivo invalido: ") + error.what();
            }
        }
        catalog.push_back(std::move(entry));
    }
    return catalog;
}

}  // namespace circuit_escape
