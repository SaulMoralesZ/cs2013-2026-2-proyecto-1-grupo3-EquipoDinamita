#pragma once

#include "./controllers.hpp"
#include "./environment.hpp"
#include "./events.hpp"

namespace circuit_escape {

// Un turno completo en el orden de la seccion 3.2:
//   1. actua el jugador            (step_player)
//   2. captura inicial             (dentro de step_player)
//   3. actua el vigilante          (step_guard, solo en turnos pares; decide `guard`)
//   4. resultado del turno         (finish_turn)
// Si guard es nullptr, el vigilante espera. Sin interruptores ni vigilante
// (mapa del Proyecto 1) equivale a NavigationEnvironment::step.
// Devuelve la observacion final y TODOS los eventos del turno, en orden.
[[nodiscard]] StepResult play_turn(NavigationEnvironment& environment, Action playerAction,
                                   IController* guard);

}  // namespace circuit_escape
