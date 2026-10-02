#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "circuit_escape/console_ui.hpp"
#include "circuit_escape/controllers.hpp"
#include "circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

struct Options {
    std::string scenario{"assets/maps/scenario_01.txt"};
    Difficulty difficulty{Difficulty::standard};
    RenderMode renderMode{RenderMode::emoji};
    std::optional<ControllerKind> controller;
    std::uint32_t seed{42};
    bool helpOnly{false};
};

void printUsage() {
    std::cout << "Uso: navigation_game [opciones]\n"
              << "  --scenario <ruta>            mapa a cargar\n"
              << "  --difficulty easy|standard|hard\n"
              << "  --ascii                      presentacion sin emojis\n"
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
        if (argument == "--scenario") {
            const auto value = next();
            if (!value) {
                std::cerr << "--scenario requiere una ruta\n";
                return std::nullopt;
            }
            options.scenario = std::string{*value};
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

template<std::size_t Rows, std::size_t Columns>
void runGame(const std::vector<std::string>& lines, const Options& options) {
    const auto scenario = parseScenario<Rows, Columns>(lines);
    NavigationEnvironment<Rows, Columns> environment(scenario.grid, scenario.start,
                                                     rulesFor(options.difficulty));
    environment.reset(options.seed);

    ConsoleUI ui(options.renderMode);
    std::unique_ptr<IController> controller;
    if (options.controller) {
        controller = makeController(*options.controller, options.seed);
    }

    std::vector<NavigationEvent> lastEvents;
    std::string notice;
    bool showHelp = false;

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    screen.TrackMouse(false);

    const auto advance = [&](Action action) {
        if (environment.isFinished()) {
            notice = "La partida termino. Q para salir.";
            return;
        }
        const auto result = environment.step(action);
        notice.clear();
        lastEvents = result.events;
    };

    auto document = ftxui::Renderer([&] {
        if (showHelp) return ui.help();

        ftxui::Elements lines_;
        lines_.push_back(ui.render(environment, lastEvents));
        if (!notice.empty()) lines_.push_back(ui.notice(notice));
        return ftxui::vbox(std::move(lines_));
    });

    document = ftxui::CatchEvent(document, [&](const ftxui::Event& event) {
        if (!ConsoleUI::isKeyboardEvent(event)) return false;
        const auto command = ui.translate(event);
        if (!command) {
            notice = "Comando desconocido. WASD mover, E esperar, H ayuda, Q salir.";
            return true;
        }

        return std::visit(
            Overloaded{
                [&](QuitCommand) {
                    screen.Exit();
                    return true;
                },
                [&](HelpCommand) {
                    showHelp = !showHelp;
                    notice.clear();
                    return true;
                },
                [&](Action action) {
                    if (showHelp) {
                        showHelp = false;
                        return true;
                    }
                    if (controller) {
                        if (environment.isFinished()) {
                            notice = "La partida termino. Q para salir.";
                            return true;
                        }
                        const auto state = environment.state();
                        advance(controller->selectAction(state, state.availableActions));
                        return true;
                    }
                    advance(action);
                    return true;
                }},
            *command);
    });

    screen.Loop(document);

    const auto final_ = environment.state();
    std::cout << "Resultado: " << toString(environment.endReason()) << '\n'
              << "Turnos: " << final_.turn << '\n'
              << "Energia restante: " << final_.energy << '/' << final_.maximumEnergy << '\n'
              << "Recursos: " << final_.collectedResources << '/'
              << environment.totalResources() << '\n'
              << "Puntaje: " << final_.score << '\n';
}

}
int main(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);
    if (!options) return 1;
    if (options->helpOnly) return 0;

    try {
        const auto lines = readLines(options->scenario);
        if (lines.empty()) {
            std::cerr << "El escenario esta vacio: " << options->scenario << '\n';
            return 1;
        }

        const std::size_t rows = lines.size();
        const std::size_t columns = lines.front().size();

        if (rows == 20 && columns == 30) {
            runGame<20, 30>(lines, *options);
        } else if (rows == 10 && columns == 20) {
            runGame<10, 20>(lines, *options);
        } else {
            std::cerr << "Dimensiones no soportadas: " << rows << "x" << columns
                      << ". El escenario de demostracion es 20x30.\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
