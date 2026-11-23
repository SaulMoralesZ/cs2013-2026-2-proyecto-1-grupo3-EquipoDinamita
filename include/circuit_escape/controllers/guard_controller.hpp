#pragma once

#include <memory>
#include <span>

#include "../cells.hpp"
#include "../controllers.hpp"
#include "../events.hpp"
#include "../graph/graph.hpp"
#include "../grid.hpp"

namespace circuit_escape {

// Politica del vigilante (seccion 3.1): conoce la posicion ACTUAL del jugador
// y lo persigue por un camino con menos movimientos (BFS).
//  - Recalcula en cada decision: no sigue un recorrido fijo.
//  - Respeta muros y bordes (el grafo solo tiene casillas transitables).
//  - Desempate fijo: orden de vecinos arriba, abajo, izquierda, derecha.
//  - Sin camino (o ya sobre el jugador): espera.
// Reutiliza el contrato del Proyecto 1: cumple NavigationPolicy y se adapta
// a IController con PolicyController. Solo propone; el motor valida y aplica.
class GuardPolicy {
public:
    // El grafo se construye una vez por mapa: los muros no cambian en la partida.
    explicit GuardPolicy(std::shared_ptr<const Graph> graph);

    // observation.guard = posicion propia; observation.agent = jugador;
    // legalActions = movimientos legales del vigilante (guard_legal_actions()).
    Action selectAction(const Observation& observation, std::span<const Action> legalActions);

private:
    std::shared_ptr<const Graph> graph_;
};

static_assert(NavigationPolicy<GuardPolicy>);

// Controlador del vigilante listo para usar sobre el mapa dado.
[[nodiscard]] std::unique_ptr<IController> make_guard_controller(const Grid<Cell>& grid);

}  // namespace circuit_escape
