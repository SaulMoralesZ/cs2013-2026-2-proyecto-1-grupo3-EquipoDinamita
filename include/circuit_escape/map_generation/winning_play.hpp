#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../events.hpp"
#include "../position.hpp"
#include "./map.hpp"

namespace circuit_escape {

// Reproduccion de una secuencia de acciones con el MOTOR REAL
// (NavigationEnvironment + play_turn + vigilante con BFS). Es la unica
// autoridad sobre si una partida gana: la busqueda solo propone.
struct ReplayOutcome {
    bool won{false};
    EndReason reason{EndReason::none};
    int final_energy{0};
    std::size_t turns{0};
    std::size_t batteries_used{0};
    std::size_t actions_played{0};  // < total si la partida termino antes
    std::string error;              // el motor rechazo el escenario (vacio si no)
};

[[nodiscard]] ReplayOutcome replay_winning_play(const Map& map,
                                                const std::vector<Action>& actions);

struct WinningPlayLimits {
    // Estados expandidos como maximo antes de rendirse.
    std::size_t max_expanded{400000};
};

struct WinningPlayResult {
    bool found{false};
    std::vector<Action> actions;  // secuencia ganadora (verificada con el motor)
    ReplayOutcome replay;         // resultado de reproducirla con el motor
    std::size_t expanded{0};
    std::string reason;           // por que no se encontro (si found es false)
};

// Busca una partida ganadora con las reglas completas: activar A y B en
// cualquier orden y llegar a la salida con energia positiva, dentro del
// limite de turnos, sin ser capturado, usando baterias si hace falta.
// Determinista: el mismo mapa produce siempre la misma secuencia.
[[nodiscard]] WinningPlayResult find_winning_play(const Map& map,
                                                  const WinningPlayLimits& limits = {});

}  // namespace circuit_escape
