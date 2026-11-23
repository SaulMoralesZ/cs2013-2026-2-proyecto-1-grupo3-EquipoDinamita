#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "./events.hpp"
#include "./map_generation/map.hpp"
#include "./position.hpp"

namespace circuit_escape {

// Partida jugada por una persona, guardada para reproducirla despues.
// Como el motor y el vigilante son deterministas, basta guardar las acciones
// de cada turno: al reproducirlas sobre el MISMO mapa se obtiene la misma
// partida. Por eso el archivo incluye el mapa completo (si el lote se
// regenera, mapa_facil.txt cambia, pero la partida sigue siendo reproducible).
struct GameRecord {
    Map map;
    std::vector<Action> actions;  // una por turno jugado, en orden
    EndReason result{EndReason::none};
    std::size_t turns{0};
    int final_energy{0};
    std::string date;             // "2026-11-23 18:40:12" (hora local)
};

// Formato de texto: cabecera "clave: valor", "acciones: RRUDW..." y despues
// "mapa:" seguido del escenario en el formato de map_io.
[[nodiscard]] std::string serialize_record(const GameRecord& record);
[[nodiscard]] GameRecord deserialize_record(const std::string& text);

// Guarda en `directory` (la crea si no existe) con nombre por fecha:
// partida_AAAAMMDD_HHMMSS.txt. Devuelve la ruta escrita.
std::filesystem::path save_record(GameRecord record, const std::filesystem::path& directory);
[[nodiscard]] GameRecord load_record(const std::filesystem::path& path);

// Fecha y hora local actuales, "AAAA-MM-DD HH:MM:SS".
[[nodiscard]] std::string current_date();

struct SavedGameEntry {
    std::filesystem::path path;
    std::optional<GameRecord> record;
    std::string error;  // archivo invalido

    // "victoria en 28 turnos, energia 31 | Facil | mapa 3 10x15"
    [[nodiscard]] std::string summary() const;
};

// Partidas de `directory`, la mas reciente primero. Nunca lanza.
[[nodiscard]] std::vector<SavedGameEntry> list_saved_games(const std::filesystem::path& directory);

}  // namespace circuit_escape
