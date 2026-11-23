#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "circuit_escape/console_ui.hpp"
#include "circuit_escape/controllers.hpp"
#include "circuit_escape/environment.hpp"
#include "circuit_escape/map_io/scenario_format.hpp"
#include "circuit_escape/paths.hpp"
#include "circuit_escape/terminal_launcher.hpp"
#include "circuit_escape/ui/game_application.hpp"

using namespace circuit_escape;

namespace {

struct Options {
    std::optional<std::string> scenario;  // mapa del Proyecto 1
    std::optional<std::string> map;    // escenario del Proyecto 2 (map_generator)
    std::optional<std::string> level;  // facil | medio | dificil dentro de mapsDir
    std::optional<std::string> mapsDir;   // por defecto: default_maps_dir_for_reading()
    std::optional<std::string> savesDir;  // por defecto: default_saves_dir()
    Difficulty difficulty{Difficulty::standard};
    std::optional<RenderMode> renderMode;  // por defecto: segun la consola
    bool launched{false};                  // relanzado en su propia terminal
    std::optional<ControllerKind> controller;
    std::uint32_t seed{42};
    bool helpOnly{false};
};

void printUsage() {
    std::cout << "Uso: escape_circuit [opciones]\n"
              << "  (sin opciones)               menu: jugar, partidas guardadas, solucion, reglas\n"
              << "  --maps-dir <carpeta>         lote de niveles (por defecto: mapas incluidos)\n"
              << "  --saves-dir <carpeta>        partidas guardadas (por defecto: carpeta del usuario)\n"
              << "  --level facil|medio|dificil  juega assets/maps/mapa_<nivel>.txt\n"
              << "  --map <ruta>                 escenario generado (interruptores A/B y vigilante)\n"
              << "  --scenario <ruta>            mapa del Proyecto 1 (sin interruptores)\n"
              << "  --difficulty easy|standard|hard\n"
              << "  --ascii | --emoji            presentacion sin / con emojis\n"
              << "  --controller random|heuristic  juega una politica automatica\n"
              << "  --seed <numero>              semilla de la politica aleatoria\n"
              << "  --help                       muestra esta ayuda\n";
}

std::optional<Options> parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= argc) return std::nullopt;
            return std::string_view{argv[++index]};
        };

        if (argument == "--help" || argument == "-h") {
            printUsage();
            options.helpOnly = true;
            return options;
        }
        if (argument == "--ascii") {
            options.renderMode = RenderMode::ascii;
            continue;
        }
        if (argument == "--emoji") {
            options.renderMode = RenderMode::emoji;
            continue;
        }
        if (argument == kLaunchedFlag) {
            options.launched = true;
            continue;
        }
        if (argument == "--scenario") {
            const auto value = next();
            if (!value) {
                std::cerr << "--scenario requiere una ruta\n";
                return std::nullopt;
            }
            options.scenario = std::string{*value};
            continue;
        }
        if (argument == "--saves-dir") {
            const auto value = next();
            if (!value) {
                std::cerr << "--saves-dir requiere una carpeta\n";
                return std::nullopt;
            }
            options.savesDir = std::string{*value};
            continue;
        }
        if (argument == "--maps-dir") {
            const auto value = next();
            if (!value) {
                std::cerr << "--maps-dir requiere una carpeta\n";
                return std::nullopt;
            }
            options.mapsDir = std::string{*value};
            continue;
        }
        if (argument == "--level") {
            const auto value = next();
            if (!value || (*value != "facil" && *value != "medio" && *value != "dificil")) {
                std::cerr << "--level espera facil, medio o dificil\n";
                return std::nullopt;
            }
            options.level = std::string{*value};
            continue;
        }
        if (argument == "--map") {
            const auto value = next();
            if (!value) {
                std::cerr << "--map requiere una ruta\n";
                return std::nullopt;
            }
            options.map = std::string{*value};
            continue;
        }
        if (argument == "--difficulty") {
            const auto value = next();
            const auto parsed = value ? parseDifficulty(*value) : std::nullopt;
            if (!parsed) {
                std::cerr << "--difficulty espera easy, standard o hard\n";
                return std::nullopt;
            }
            options.difficulty = *parsed;
            continue;
        }
        if (argument == "--controller") {
            const auto value = next();
            const auto parsed = value ? parseControllerKind(*value) : std::nullopt;
            if (!parsed) {
                std::cerr << "--controller espera random o heuristic\n";
                return std::nullopt;
            }
            options.controller = *parsed;
            continue;
        }
        if (argument == "--seed") {
            const auto value = next();
            if (!value) {
                std::cerr << "--seed requiere un numero\n";
                return std::nullopt;
            }
            options.seed = static_cast<std::uint32_t>(std::strtoul(std::string{*value}.c_str(),
                                                                  nullptr, 10));
            continue;
        }

        std::cerr << "Opcion desconocida: " << argument << '\n';
        printUsage();
        return std::nullopt;
    }
    return options;
}

}
int main(int argc, char** argv) {
    prepare_console();
    const std::vector<std::string> args(argv + 1, argv + argc);

    // Doble clic (sin terminal): el juego se vuelve a abrir en su propia terminal.
    const bool hasLaunchedFlag =
        std::find(args.begin(), args.end(), std::string(kLaunchedFlag)) != args.end();
    if (!running_in_terminal() && !hasLaunchedFlag) {
        relaunch_in_terminal(args);  // solo vuelve si no hay ninguna terminal
        std::cerr << "No se encontro una terminal para abrir el juego. Ejecute escape_circuit "
                     "desde una terminal.\n";
        return 1;
    }

    const auto options = parseOptions(argc, argv);
    if (!options) {
        pause_before_closing(hasLaunchedFlag);
        return 1;
    }
    if (options->helpOnly) return 0;

    try {
        const std::filesystem::path mapsDir =
            options->mapsDir ? std::filesystem::path(*options->mapsDir)
                             : default_maps_dir_for_reading();
        const std::filesystem::path savesDir =
            options->savesDir ? std::filesystem::path(*options->savesDir) : default_saves_dir();
        const RenderMode mode = options->renderMode.value_or(
            console_supports_emoji() ? RenderMode::emoji : RenderMode::ascii);

        GameApplication app(AppOptions{mode, options->controller, options->seed, mapsDir,
                                       savesDir});
        std::optional<std::string> mapPath = options->map;
        if (options->level) {
            mapPath = (mapsDir /
                       ("mapa_" + *options->level + ".txt"))
                          .string();
        }

        if (mapPath) {
            // Proyecto 2: dimensiones, reglas y objetivos vienen del archivo.
            const Map map = loadMap(*mapPath);
            std::cout << "Mapa " << map.id << " (nivel " << toString(map.level) << ", costo minimo "
                      << map.minCost << ", " << map.grid.rows() << "x" << map.grid.columns()
                      << ")\n";
            NavigationEnvironment environment(to_scenario(map));
            app.play(environment, false, &map);
        } else if (options->scenario) {
            const auto lines = readLines(*options->scenario);
            if (lines.empty()) {
                std::cerr << "El escenario esta vacio: " << *options->scenario << '\n';
                pause_before_closing(options->launched);
                return 1;
            }
            const auto layout = parseScenario(lines);
            NavigationEnvironment environment(layout.grid, layout.start,
                                              rulesFor(options->difficulty));
            app.play(environment, false);
        } else {
            app.run();  // menu principal
        }
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        pause_before_closing(options->launched);
        return 1;
    }
    return 0;
}