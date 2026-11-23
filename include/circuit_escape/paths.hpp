#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace circuit_escape {

// Ubicacion de los archivos del juego, independiente de la carpeta desde la
// que se lance (doble clic, menu de aplicaciones, terminal).
//
//  - Recursos incluidos (solo lectura): mapas por nivel que se entregan con
//    el juego, buscados RELATIVOS AL EJECUTABLE:
//      <exe>/assets                          carpeta portable
//      <exe>/../share/escape_circuit/assets  instalado (/usr/bin -> /usr/share)
//      <exe>/../assets                       repositorio (build/escape_circuit)
//  - Datos del usuario (escritura): partidas guardadas y mapas generados.
//      Linux:   $XDG_DATA_HOME/escape_circuit  o  ~/.local/share/escape_circuit
//      Windows: %APPDATA%\escape_circuit
// La variable CIRCUIT_ESCAPE_DATA permite forzar la carpeta de recursos.

// Carpeta del ejecutable en curso (vacia si no se puede determinar).
[[nodiscard]] std::filesystem::path executable_dir();

// Carpetas candidatas de recursos, en orden de prioridad.
[[nodiscard]] std::vector<std::filesystem::path> asset_dir_candidates();

// Primera carpeta de recursos que tiene "maps/"; nullopt si ninguna.
[[nodiscard]] std::optional<std::filesystem::path> find_asset_dir();

// Carpeta de datos del usuario (no se crea).
[[nodiscard]] std::filesystem::path user_data_dir();

// Partidas guardadas: <usuario>/saves.
[[nodiscard]] std::filesystem::path default_saves_dir();

// Mapas que lee el juego: los generados por el usuario si tiene los tres
// niveles; si no, los incluidos; si no hay ninguno, la carpeta del usuario
// (el menu mostrara que faltan).
[[nodiscard]] std::filesystem::path default_maps_dir_for_reading();

// Donde guarda map_generator: la carpeta de mapas incluida si se puede
// escribir (repositorio o carpeta portable); si no (instalado), la del usuario.
[[nodiscard]] std::filesystem::path default_maps_dir_for_writing();

// true si se pueden crear archivos en `directory` (la crea si hace falta).
[[nodiscard]] bool is_writable_dir(const std::filesystem::path& directory);

// true si `directory` tiene mapa_facil.txt, mapa_medio.txt y mapa_dificil.txt.
[[nodiscard]] bool has_level_files(const std::filesystem::path& directory);

}  // namespace circuit_escape
