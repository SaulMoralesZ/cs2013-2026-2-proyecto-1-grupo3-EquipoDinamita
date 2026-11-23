#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "./map.hpp"
#include "./map_selector.hpp"
#include "./map_validator.hpp"
#include "./winning_play.hpp"

namespace circuit_escape {

// Configuracion de un lote: todas las alternativas comparten dimensiones y
// reglas, para que sus costos sean comparables (seccion 2.4).
struct BatchConfig {
    int rows{10};
    int cols{15};
    unsigned seed{42};      // el mapa i (id i) usa seed + i - 1
    int count{10};          // alternativas: 0..10
    std::size_t workers{1}; // trabajadores std::jthread (1 = secuencial)
    MapConfig map{};
    WinningPlayLimits search{};
};

// Construye una alternativa a partir de su id y semilla (reemplazable en pruebas).
using MapBuildFn = std::function<Map(int id, unsigned seed)>;

// Resultado de una alternativa. Cada tarea escribe solo en su propio informe.
struct MapReport {
    int id{0};
    unsigned seed{0};
    std::optional<Map> map;                    // construido (fase 1)
    std::optional<MapValidation> validation;   // rutas y costos (fase 2)
    std::optional<WinningPlayResult> winning;  // victoria (fase 2)
    bool accepted{false};                      // apto para clasificar (fase 3)
    std::string verdict;                       // motivo legible del resultado
    std::string error;                         // excepcion capturada en este mapa
    std::size_t build_worker{0};               // informativo: que hilo lo hizo
    std::size_t validate_worker{0};
};

struct BatchResult {
    std::vector<MapReport> reports;  // ordenados por id
    LevelSelection selection;
    // Mapa elegido por nivel, con su `level` asignado: facil, medio, dificil.
    std::array<std::optional<Map>, 3> levels;
    std::size_t workers{0};
    std::chrono::duration<double, std::milli> build_time{0};
    std::chrono::duration<double, std::milli> validation_time{0};
    std::chrono::duration<double, std::milli> selection_time{0};
};

// Construye una alternativa con Kruskal (fase 1).
[[nodiscard]] Map build_map(const BatchConfig& config, int id, unsigned seed);

// Valida rutas y costos con Dijkstra y busca una partida ganadora (fase 2).
// Solo lee report.map; escribe en el resto de report.
void validate_alternative(MapReport& report, const WinningPlayLimits& search);

// Coordina el lote (seccion 6):
//   1. construccion en paralelo  -> espera a todas
//   2. validacion en paralelo    -> espera a todas
//   3. comprobacion de victorias y clasificacion por nivel, en orden
// Con las mismas semillas, dimensiones y reglas, 1 o N trabajadores producen
// los mismos mapas, costos, recorridos y niveles por identificador.
[[nodiscard]] BatchResult run_map_batch(const BatchConfig& config);
[[nodiscard]] BatchResult run_map_batch(const BatchConfig& config, const MapBuildFn& build);

}  // namespace circuit_escape
