// map_generator: construye mapas con Kruskal y los guarda como escenarios.
//
// Lote de hasta diez alternativas: construccion (Kruskal), validacion de rutas
// y costos (Dijkstra), partida ganadora comprobada con el motor, clasificacion
// por costo y un archivo por nivel (mapa_facil/medio/dificil.txt).
// Construccion y validacion se reparten entre trabajadores std::jthread
// (MapBatchRunner, seccion 6); clasificacion en orden.

#include <cstdlib>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "circuit_escape/map_generation/map_batch_runner.hpp"
#include "circuit_escape/map_generation/map_builder.hpp"
#include "circuit_escape/map_generation/parallel.hpp"
#include "circuit_escape/map_io/scenario_format.hpp"
#include "circuit_escape/paths.hpp"
#include "circuit_escape/ui/level_catalog.hpp"

using namespace circuit_escape;

namespace {

struct Options {
    int rows{10};
    int cols{15};
    unsigned seed{42};
    int count{10};
    std::size_t workers{default_workers()};
    std::string outDir{default_maps_dir_for_writing().string()};
    MapConfig config{};
};

void printUsage() {
    std::cout << "Uso: map_generator [opciones]\n"
              << "  --rows <n>         filas (por defecto 10)\n"
              << "  --cols <n>         columnas (por defecto 15)\n"
              << "  --seed <n>         semilla base; el mapa i usa seed + i - 1\n"
              << "  --count <n>        alternativas del lote, 1..10 (por defecto 10)\n"
              << "  --workers <n>      trabajadores std::jthread (por defecto: nucleos)\n"
              << "  --out <dir>        carpeta de salida (por defecto: mapas del juego si se pueden\n"
              << "                     escribir; si no, la carpeta del usuario)\n"
              << "  --energy <n>       energia inicial y maxima\n"
              << "  --turns <n>        limite de turnos\n"
              << "  --resources <n>    recursos a colocar\n"
              << "  --batteries <n>    baterias a colocar\n"
              << "  --traps <n>        trampas a colocar\n";
}

std::optional<long> parseNumber(std::string_view text) {
    if (text.empty()) return std::nullopt;
    char* end = nullptr;
    const std::string copy{text};
    const long value = std::strtol(copy.c_str(), &end, 10);
    if (end == copy.c_str() || *end != '\0') return std::nullopt;
    return value;
}

std::optional<Options> parseOptions(int argc, char** argv) {
    Options o;
    o.config.numResources = 3;
    o.config.numBatteries = 2;
    o.config.numTraps = 2;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        }
        if (i + 1 >= argc) {
            std::cerr << "Falta el valor de " << arg << '\n';
            return std::nullopt;
        }
        const std::string_view value{argv[++i]};
        if (arg == "--out") {
            o.outDir = std::string{value};
            continue;
        }
        const auto n = parseNumber(value);
        if (!n) {
            std::cerr << arg << " espera un numero entero, se recibio '" << value << "'\n";
            return std::nullopt;
        }
        if (arg == "--rows") o.rows = static_cast<int>(*n);
        else if (arg == "--cols") o.cols = static_cast<int>(*n);
        else if (arg == "--seed") o.seed = static_cast<unsigned>(*n);
        else if (arg == "--count") o.count = static_cast<int>(*n);
        else if (arg == "--workers") {
            if (*n < 1) {
                std::cerr << "ERROR: --workers debe ser al menos 1\n";
                return std::nullopt;
            }
            o.workers = static_cast<std::size_t>(*n);
        }
        else if (arg == "--energy") o.config.energyInitial = o.config.energyMax = static_cast<int>(*n);
        else if (arg == "--turns") o.config.turnLimit = static_cast<std::size_t>(*n);
        else if (arg == "--resources") o.config.numResources = static_cast<int>(*n);
        else if (arg == "--batteries") o.config.numBatteries = static_cast<int>(*n);
        else if (arg == "--traps") o.config.numTraps = static_cast<int>(*n);
        else {
            std::cerr << "Opcion desconocida: " << arg << '\n';
            printUsage();
            return std::nullopt;
        }
    }

    if (const auto error = MapBuilder::validateDimensions(o.rows, o.cols)) {
        std::cerr << "ERROR: " << *error << '\n';
        return std::nullopt;
    }
    if (o.count < 1 || o.count > 10) {
        std::cerr << "ERROR: --count debe estar entre 1 y 10\n";
        return std::nullopt;
    }
    if (o.config.energyInitial <= 0 || o.config.turnLimit == 0) {
        std::cerr << "ERROR: energia y limite de turnos deben ser positivos\n";
        return std::nullopt;
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);
    if (!options) return 1;

    try {
        std::filesystem::create_directories(options->outDir);

        BatchConfig batch;
        batch.rows = options->rows;
        batch.cols = options->cols;
        batch.seed = options->seed;
        batch.count = options->count;
        batch.workers = options->workers;
        batch.map = options->config;
        const BatchResult result = run_map_batch(batch);

        std::cout << "Lote de " << options->count << " alternativas " << options->rows << "x"
                  << options->cols << " (semilla base " << options->seed << ") con "
                  << result.workers << " trabajador(es)\n";
        for (const MapReport& r : result.reports) {
            std::cout << "Mapa " << r.id << " (semilla " << r.seed << ") construido por T"
                      << r.build_worker;
            if (r.validate_worker) std::cout << ", validado por T" << r.validate_worker;
            std::cout << '\n';
            if (r.map) {
                std::cout << "  kruskal: arbol " << r.map->tree_openings.size()
                          << " aperturas, " << r.map->extra_openings.size() << " adicionales\n";
            }
            if (r.validation) {
                std::cout << "  validacion: " << toString(r.validation->status) << " - "
                          << r.validation->diagnosis << '\n';
            }
            if (r.winning && r.winning->found) {
                std::cout << "  partida ganadora: " << r.winning->actions.size()
                          << " turnos, energia final " << r.winning->replay.final_energy
                          << ", baterias usadas " << r.winning->replay.batteries_used << " ("
                          << r.winning->expanded << " estados explorados)\n";
            }
            std::cout << "  resultado: " << (r.accepted ? "ACEPTADO" : "RECHAZADO") << " - "
                      << r.verdict << '\n';
        }

        const auto accepted = std::count_if(result.reports.begin(), result.reports.end(),
                                            [](const MapReport& r) { return r.accepted; });
        std::cout << "\nTiempos: construccion " << result.build_time.count() << " ms, validacion "
                  << result.validation_time.count() << " ms, clasificacion "
                  << result.selection_time.count() << " ms\n";
        std::cout << "Mapas con victoria comprobada: " << accepted << " de " << options->count;
        if (accepted > 0) std::cout << " (costo promedio " << result.selection.average << ")";
        std::cout << '\n';
        if (!result.selection.ok()) {
            std::cerr << "ERROR: " << result.selection.error << '\n';
            return 1;
        }

        for (const auto& level : result.levels) {
            const Map& map = *level;
            const auto path = std::filesystem::path(options->outDir) / level_file_name(map.level);
            saveMap(map, path.string());
            std::cout << "  " << level_label(map.level) << ": mapa " << map.id << " (costo "
                      << map.minCost << ") -> " << path.string() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
