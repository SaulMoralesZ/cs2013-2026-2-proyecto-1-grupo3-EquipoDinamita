#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace circuit_escape {

// Mapa revisado que tiene una partida ganadora guardada y reproducible.
struct MapCandidate {
    int id;        // identificador asignado antes de generar el lote (1, 2, 3...)
    int min_cost;  // costo del mapa: menor recorrido completo (Dijkstra)
};

// Un mapa por nivel (seccion 2.4), identificado por su id.
struct LevelSelection {
    std::optional<int> easy;    // menor costo
    std::optional<int> medium;  // costo mas cercano al promedio, sin el minimo ni el maximo
    std::optional<int> hard;    // mayor costo
    double average{0.0};        // promedio de los costos minimos del lote
    std::size_t distinct_costs{0};
    std::string error;          // vacio si se pudieron elegir los tres niveles

    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

// Clasifica por costo de energia (no por los valores aleatorios de Kruskal).
// Empates de costo o de distancia al promedio: gana el id menor. El resultado
// no depende del orden de entrada (ni de que trabajador termino primero).
// Errores: lote vacio, o menos de tres costos distintos.
[[nodiscard]] LevelSelection select_levels(std::span<const MapCandidate> candidates);

}  // namespace circuit_escape
