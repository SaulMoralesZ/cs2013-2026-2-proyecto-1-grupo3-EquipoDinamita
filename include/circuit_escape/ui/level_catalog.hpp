#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../game_rules.hpp"
#include "../map_generation/map.hpp"

namespace circuit_escape {

// Nombre del archivo de cada nivel dentro de la carpeta de mapas:
// mapa_facil.txt, mapa_medio.txt, mapa_dificil.txt (seccion 2.5).
[[nodiscard]] std::string level_file_name(Difficulty level);
// "Facil", "Medio", "Dificil".
[[nodiscard]] std::string level_label(Difficulty level);

// Un nivel del lote preparado: el mapa cargado o el motivo por el que no se puede jugar.
struct LevelEntry {
    Difficulty level{Difficulty::easy};
    std::string label;
    std::filesystem::path path;
    std::optional<Map> map;  // cargado y aceptado por el motor
    std::string error;       // archivo ausente o invalido

    [[nodiscard]] bool available() const noexcept { return map.has_value(); }
    [[nodiscard]] bool has_winning_play() const noexcept {
        return map && !map->winning_play.empty();
    }
    // "mapa 3 | 10x15 | costo minimo 22 | energia 60 | 180 turnos"
    [[nodiscard]] std::string summary() const;
};

// Lee los tres niveles de `directory`, siempre en orden facil, medio, dificil.
// Nunca lanza: un archivo ausente o invalido queda registrado en su entrada.
[[nodiscard]] std::vector<LevelEntry> load_level_catalog(const std::filesystem::path& directory);

// Cursor de un menu vertical: arriba/abajo con vuelta al otro extremo.
class MenuCursor {
public:
    explicit MenuCursor(std::size_t items) : items_(items) {}
    void up() noexcept { if (items_) selected_ = (selected_ + items_ - 1) % items_; }
    void down() noexcept { if (items_) selected_ = (selected_ + 1) % items_; }
    void select(std::size_t index) noexcept { if (index < items_) selected_ = index; }
    [[nodiscard]] std::size_t selected() const noexcept { return selected_; }

private:
    std::size_t items_;
    std::size_t selected_{0};
};

}  // namespace circuit_escape
