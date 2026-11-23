#include "../../include/circuit_escape/map_generation/map_batch_runner.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "../../include/circuit_escape/map_generation/map_builder.hpp"
#include "../../include/circuit_escape/map_generation/parallel.hpp"

namespace circuit_escape {

namespace {

using Clock = std::chrono::steady_clock;

std::string describe_exception(std::exception_ptr error) {
    try {
        std::rethrow_exception(std::move(error));
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "error desconocido";
    }
}

}  // namespace

Map build_map(const BatchConfig& config, int id, unsigned seed) {
    return MapBuilder::buildMap(id, seed, config.rows, config.cols, config.map);
}

void validate_alternative(MapReport& report, const WinningPlayLimits& search) {
    const Map& map = *report.map;
    report.validation = validate_map(map);
    const MapValidation& v = *report.validation;
    if (!v.usable()) {
        report.verdict = "rechazado en validacion: " + v.diagnosis;
        return;
    }
    report.winning = find_winning_play(map, search);
    const WinningPlayResult& win = *report.winning;
    if (!win.found) {
        report.verdict = "sin partida ganadora: " + win.reason;
        return;
    }
    if (v.status == ValidationStatus::needs_recharge_proof && win.replay.batteries_used == 0) {
        report.verdict = "la partida ganadora no demuestra el uso de recargas";
        return;
    }
    report.verdict = "victoria en " + std::to_string(win.actions.size()) + " turnos";
}

BatchResult run_map_batch(const BatchConfig& config) {
    return run_map_batch(config, [&config](int id, unsigned seed) {
        return build_map(config, id, seed);
    });
}

BatchResult run_map_batch(const BatchConfig& config, const MapBuildFn& build) {
    BatchResult result;
    const std::size_t count = config.count > 0 ? static_cast<std::size_t>(config.count) : 0;

    // Identificadores y semillas se fijan ANTES de repartir tareas, y cada
    // resultado tiene su posicion exclusiva reservada de antemano.
    result.reports.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.reports[i].id = static_cast<int>(i) + 1;
        result.reports[i].seed = config.seed + static_cast<unsigned>(i);
    }

    // ---- Fase 1: construccion en paralelo (cada tarea con su propio rng y Kruskal).
    auto start = Clock::now();
    result.workers = run_parallel(count, config.workers, [&](std::size_t i, std::size_t worker) {
        MapReport& report = result.reports[i];
        report.build_worker = worker + 1;
        try {
            report.map = build(report.id, report.seed);
        } catch (...) {
            report.error = "construccion: " + describe_exception(std::current_exception());
            report.verdict = report.error;
        }
    });
    result.build_time = Clock::now() - start;

    // ---- Fase 2: validacion en paralelo, solo despues de terminar TODA la construccion.
    // Cada tarea lee su mapa terminado y usa sus propias distancias y colas.
    start = Clock::now();
    (void)run_parallel(count, config.workers, [&](std::size_t i, std::size_t worker) {
        MapReport& report = result.reports[i];
        if (!report.map) return;
        report.validate_worker = worker + 1;
        try {
            validate_alternative(report, config.search);
        } catch (...) {
            report.error = "validacion: " + describe_exception(std::current_exception());
            report.verdict = report.error;
        }
    });
    result.validation_time = Clock::now() - start;

    // ---- Fase 3 (en orden): comprobar victorias y clasificar por nivel.
    start = Clock::now();
    std::vector<MapCandidate> candidates;
    for (MapReport& report : result.reports) {
        if (!report.error.empty() || !report.winning || !report.winning->found) continue;
        if (report.validation->status == ValidationStatus::needs_recharge_proof &&
            report.winning->replay.batteries_used == 0) {
            continue;
        }
        // Partida ganadora guardada y reproducible: se vuelve a jugar con el motor.
        if (!replay_winning_play(*report.map, report.winning->actions).won) {
            report.verdict = "la partida ganadora no se reproduce";
            continue;
        }
        report.map->minCost = *report.validation->min_cost;
        report.map->winning_play = report.winning->actions;
        report.accepted = true;
        candidates.push_back({report.id, report.map->minCost});
    }

    result.selection = select_levels(candidates);
    if (result.selection.ok()) {
        const std::array<std::pair<std::optional<int>, Difficulty>, 3> chosen{{
            {result.selection.easy, Difficulty::easy},
            {result.selection.medium, Difficulty::standard},
            {result.selection.hard, Difficulty::hard},
        }};
        for (std::size_t level = 0; level < chosen.size(); ++level) {
            const auto& report = result.reports[static_cast<std::size_t>(*chosen[level].first - 1)];
            result.levels[level] = *report.map;
            result.levels[level]->level = chosen[level].second;
        }
    }
    result.selection_time = Clock::now() - start;
    return result;
}

}  // namespace circuit_escape
