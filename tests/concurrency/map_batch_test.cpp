#undef NDEBUG
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "circuit_escape/map_generation/map_batch_runner.hpp"
#include "circuit_escape/map_generation/parallel.hpp"

using namespace circuit_escape;

namespace {

BatchConfig smallBatch(std::size_t workers) {
    BatchConfig c;
    c.rows = 10;
    c.cols = 15;
    c.seed = 42;
    c.count = 10;
    c.workers = workers;
    c.map.numResources = 3;
    c.map.numBatteries = 2;
    c.map.numTraps = 2;
    return c;
}

bool sameGrid(const Map& a, const Map& b) {
    if (a.grid.rows() != b.grid.rows() || a.grid.columns() != b.grid.columns()) return false;
    for (std::size_t r = 0; r < a.grid.rows(); ++r)
        for (std::size_t c = 0; c < a.grid.columns(); ++c)
            if (a.grid.at({r, c}).index() != b.grid.at({r, c}).index()) return false;
    return true;
}

// Seccion 6.5: mismos mapas, costos, recorridos y niveles con 1 y N trabajadores.
void sameResultsWithOneOrManyWorkers() {
    const BatchResult base = run_map_batch(smallBatch(1));
    assert(base.workers == 1);
    assert(base.selection.ok());
    for (std::size_t workers : {2u, 3u, 4u, 10u, 16u}) {
        const BatchResult other = run_map_batch(smallBatch(workers));
        assert(other.reports.size() == base.reports.size());
        for (std::size_t i = 0; i < base.reports.size(); ++i) {
            const MapReport& a = base.reports[i];
            const MapReport& b = other.reports[i];
            assert(a.id == b.id && a.seed == b.seed);
            assert(a.accepted == b.accepted);
            assert(a.verdict == b.verdict);
            assert(sameGrid(*a.map, *b.map));
            assert(a.map->start == b.map->start && a.map->exit == b.map->exit);
            assert(a.map->tree_openings == b.map->tree_openings);
            assert(a.validation->min_cost == b.validation->min_cost);
            assert(a.validation->routes.best == b.validation->routes.best);
            if (a.validation->routes.best_plan()) {
                assert(a.validation->routes.best_plan()->full_path() ==
                       b.validation->routes.best_plan()->full_path());
            }
            assert(a.winning.has_value() == b.winning.has_value());
            if (a.winning) assert(a.winning->actions == b.winning->actions);
        }
        assert(other.selection.easy == base.selection.easy);
        assert(other.selection.medium == base.selection.medium);
        assert(other.selection.hard == base.selection.hard);
        for (std::size_t level = 0; level < 3; ++level) {
            assert(other.levels[level]->id == base.levels[level]->id);
            assert(other.levels[level]->level == base.levels[level]->level);
        }
    }
}

// Seccion 6.1: la construccion corre realmente en paralelo.
void constructionRunsInParallel() {
    std::atomic<int> active{0};
    std::atomic<int> peak{0};
    BatchConfig config = smallBatch(4);
    config.count = 8;
    const auto slowBuild = [&](int id, unsigned seed) {
        const int now = ++active;
        int seen = peak.load();
        while (now > seen && !peak.compare_exchange_weak(seen, now)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        Map map = build_map(config, id, seed);
        --active;
        return map;
    };
    const BatchResult r = run_map_batch(config, slowBuild);
    assert(r.workers == 4);
    assert(peak.load() > 1);  // varios mapas construyendose a la vez
    std::set<std::size_t> builders;
    for (const MapReport& report : r.reports) builders.insert(report.build_worker);
    assert(builders.size() > 1);
    // Fases medidas por separado; 8 mapas de 40 ms en 4 hilos tardan ~80 ms, no 320.
    assert(r.build_time.count() >= 40.0);
    assert(r.build_time.count() < 300.0);
    assert(r.validation_time.count() >= 0.0 && r.selection_time.count() >= 0.0);
}

// Seccion 6: toda la construccion termina antes de iniciar la validacion.
// run_parallel actua de barrera: al empezar la segunda fase, la primera
// completo todas sus tareas.
void phasesAreSeparated() {
    constexpr std::size_t tasks = 64;
    std::atomic<std::size_t> built{0};
    std::atomic<bool> overlap{false};
    run_parallel(tasks, 8, [&](std::size_t, std::size_t) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        ++built;
    });
    run_parallel(tasks, 8, [&](std::size_t, std::size_t) {
        if (built.load() != tasks) overlap = true;
    });
    assert(!overlap);
}

// Cada indice se procesa exactamente una vez; cada tarea escribe en su posicion.
void eachTaskExactlyOnce() {
    for (std::size_t workers : {1u, 3u, 8u, 64u}) {
        std::vector<int> hits(1000, 0);
        const std::size_t used = run_parallel(hits.size(), workers,
                                              [&](std::size_t i, std::size_t) { ++hits[i]; });
        assert(used == std::min<std::size_t>(workers, hits.size()));
        for (int h : hits) assert(h == 1);
    }
    // Cero trabajadores pedidos -> uno solo.
    std::vector<int> hits(5, 0);
    assert(run_parallel(hits.size(), 0, [&](std::size_t i, std::size_t) { ++hits[i]; }) == 1);
}

void emptyBatch() {
    BatchConfig config = smallBatch(4);
    config.count = 0;
    const BatchResult r = run_map_batch(config);
    assert(r.reports.empty());
    assert(r.workers == 0);
    assert(!r.selection.ok());
    assert(!r.levels[0] && !r.levels[1] && !r.levels[2]);
}

// Un fallo en una tarea se registra en su mapa y no detiene a las demas.
void failingTaskIsIsolated() {
    for (std::size_t workers : {1u, 4u}) {
        BatchConfig config = smallBatch(workers);
        const auto flaky = [&](int id, unsigned seed) {
            if (id == 3) throw std::runtime_error("fallo simulado");
            return build_map(config, id, seed);
        };
        const BatchResult r = run_map_batch(config, flaky);
        assert(r.reports.size() == 10);
        const MapReport& broken = r.reports[2];
        assert(!broken.map && !broken.accepted);
        assert(broken.error.find("fallo simulado") != std::string::npos);
        assert(broken.error.find("construccion") != std::string::npos);
        for (const MapReport& report : r.reports) {
            if (report.id != 3) assert(report.map.has_value() && report.error.empty());
        }
        // El resto del lote se clasifica igual que en el lote completo sin el mapa 3.
        assert(r.selection.easy != 3 && r.selection.medium != 3 && r.selection.hard != 3);
    }
}

}  // namespace

int main() {
    eachTaskExactlyOnce();
    phasesAreSeparated();
    emptyBatch();
    failingTaskIsIsolated();
    constructionRunsInParallel();
    sameResultsWithOneOrManyWorkers();
    return 0;
}
