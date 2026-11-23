#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../pathfinding/route_planner.hpp"
#include "./map.hpp"

namespace circuit_escape {

// Resultado de validar un mapa con Dijkstra (seccion 2.3).
enum class ValidationStatus {
    // Hay recorrido completo y la energia inicial supera su costo.
    accepted,
    // Hay recorrido, la energia no alcanza sin recargas, pero hay baterias:
    // solo una partida ganadora guardada (seccion 2.4) puede habilitarlo.
    needs_recharge_proof,
    // Sin recorrido posible, o energia insuficiente sin baterias.
    rejected
};

[[nodiscard]] std::string toString(ValidationStatus status);

struct MapValidation {
    int map_id{0};
    ValidationStatus status{ValidationStatus::rejected};
    RoutePlanning routes;                  // ambos recorridos y sus tramos
    std::optional<int> min_cost;           // menor recorrido posible (costo del mapa)
    std::optional<EnergyVerdict> energy;   // energia inicial frente a min_cost
    std::size_t batteries{0};              // baterias disponibles en el mapa
    std::vector<RouteTarget> unreachable;  // objetivos inaccesibles desde el inicio
    std::string diagnosis;                 // mensaje legible (motivo del rechazo)

    [[nodiscard]] bool usable() const noexcept { return status != ValidationStatus::rejected; }
};

// Valida rutas y costos de un mapa terminado. Solo lee el mapa: no modifica
// muros ni elementos. No anticipa al vigilante ni las recargas: la victoria
// con reglas completas se comprueba aparte (seccion 2.4).
[[nodiscard]] MapValidation validate_map(const Map& map);

}  // namespace circuit_escape
