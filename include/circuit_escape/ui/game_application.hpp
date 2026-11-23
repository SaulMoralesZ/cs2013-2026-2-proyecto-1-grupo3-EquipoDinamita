#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../console_ui.hpp"
#include "../controllers.hpp"
#include "../environment.hpp"
#include "../map_generation/map.hpp"

namespace circuit_escape {

struct AppOptions {
    RenderMode mode{RenderMode::emoji};
    std::optional<ControllerKind> controller;  // politica automatica del jugador
    std::uint32_t seed{42};
    std::filesystem::path maps_dir{"assets/maps"};
    std::filesystem::path saves_dir{"saves"};  // partidas guardadas por el jugador
};

// Coordina las pantallas del juego (seccion 5.1): menu principal, eleccion de
// nivel, partida por turnos, reproduccion de la victoria guardada y reglas.
// Solo traduce teclas y dibuja: el motor sigue siendo el unico que modifica
// la partida.
class GameApplication {
public:
    explicit GameApplication(AppOptions options);

    // Menu principal: Jugar, Ver partida guardada, Ver solucion del mapa,
    // Como jugar, Salir.
    void run();

    // Una partida. Con fromMenu, ocupa la pantalla completa y Q/M vuelven al
    // menu; sin el (--map, --level), Q sale e imprime un resumen al final.
    // Si se pasa el mapa, se graban las acciones y al terminar G la guarda.
    void play(NavigationEnvironment& environment, bool fromMenu, const Map* map = nullptr);

    // Reproduce una secuencia de acciones sobre el mapa con el motor real,
    // turno a turno (solucion del mapa o partida guardada).
    void replay(const Map& map, std::vector<Action> actions, const std::string& title,
                std::optional<EndReason> expected = std::nullopt);

private:
    // Menu generico: indice elegido o nullopt si se volvio con Esc/Q.
    std::optional<std::size_t> choose(const std::string& title,
                                      const std::vector<MenuEntry>& entries,
                                      const std::string& hint, std::size_t initial = 0);
    // Elige un nivel del lote preparado en maps_dir.
    std::optional<Map> choose_level(const std::string& title, bool needWinningPlay);
    void how_to_play();
    void watch_saved_game();

    AppOptions options_;
    ConsoleUI ui_;
};

}  // namespace circuit_escape
