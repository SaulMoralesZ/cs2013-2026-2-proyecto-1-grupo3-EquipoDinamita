#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "./controllers.hpp"
#include "./environment.hpp"
#include "./events.hpp"
#include "./map_generation/map.hpp"

namespace circuit_escape {

// Reproduce la partida ganadora guardada de un mapa con el MOTOR REAL:
// NavigationEnvironment + play_turn + vigilante con BFS. Cada step() juega
// un turno de winning_play. La usan la pantalla de reproduccion del juego y
// la comprobacion de victorias del generador.
class ReplaySession {
public:
    // Reproduce la solucion del mapa (map.winning_play).
    explicit ReplaySession(const Map& map);
    // Reproduce cualquier secuencia sobre el mapa (p. ej. una partida guardada).
    ReplaySession(const Map& map, std::vector<Action> actions);

    // Juega la siguiente accion. false si ya no queda nada por jugar.
    bool step();
    // Vuelve al estado inicial (reset del motor) para repetir la reproduccion.
    void restart();

    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] std::size_t played() const noexcept { return next_; }
    [[nodiscard]] std::size_t total() const noexcept { return actions_.size(); }
    [[nodiscard]] Action next_action() const;  // solo si !finished()
    [[nodiscard]] const NavigationEnvironment& environment() const noexcept { return env_; }
    [[nodiscard]] const std::vector<NavigationEvent>& last_events() const noexcept {
        return events_;
    }

private:
    NavigationEnvironment env_;
    std::unique_ptr<IController> guard_;
    std::vector<Action> actions_;
    std::size_t next_{0};
    std::vector<NavigationEvent> events_;
};

}  // namespace circuit_escape
