#include "../../include/circuit_escape/ui/game_application.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <variant>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>

#include "../../include/circuit_escape/controllers/guard_controller.hpp"
#include "../../include/circuit_escape/game_record.hpp"
#include "../../include/circuit_escape/game_turn.hpp"
#include "../../include/circuit_escape/graph/graph.hpp"
#include "../../include/circuit_escape/map_io/scenario_format.hpp"
#include "../../include/circuit_escape/replay_session.hpp"
#include "../../include/circuit_escape/ui/energy_help.hpp"
#include "../../include/circuit_escape/ui/level_catalog.hpp"

namespace circuit_escape {

namespace {

bool is_key(const ftxui::Event& event, char lower) {
    if (!event.is_character()) return false;
    const std::string& c = event.character();
    return c.size() == 1 && (c[0] == lower || c[0] == lower - 'a' + 'A');
}

bool is_up(const ftxui::Event& e) { return e == ftxui::Event::ArrowUp || is_key(e, 'w'); }
bool is_down(const ftxui::Event& e) { return e == ftxui::Event::ArrowDown || is_key(e, 's'); }
bool is_back(const ftxui::Event& e) { return e == ftxui::Event::Escape || is_key(e, 'q'); }
bool is_accept(const ftxui::Event& e) {
    return e == ftxui::Event::Return || e == ftxui::Event::Character(' ');
}

std::string describe_action(Action action) {
    switch (action) {
        case Action::up: return "arriba";
        case Action::down: return "abajo";
        case Action::left: return "izquierda";
        case Action::right: return "derecha";
        case Action::wait: return "esperar";
    }
    return "?";
}

std::string end_summary(const NavigationEnvironment& env) {
    return "Fin: " + toString(env.endReason()) + " en el turno " + std::to_string(env.turn()) +
           ", energia " + std::to_string(env.energy());
}

// Dibuja `make()` con el zoom mas grande (<= cap) que cabe en la terminal.
// Si ni con zoom 1 cabe, lo deja en 1 (quien llama muestra el aviso).
template<typename Make>
ftxui::Element fit_zoom(ConsoleUI& ui, int cap, Make make) {
    const auto terminal = ftxui::Terminal::Size();
    for (int zoom = cap; zoom > 1; --zoom) {
        ui.setZoom(zoom);
        auto element = make();
        element->ComputeRequirement();
        const auto& need = element->requirement();
        if (need.min_x <= terminal.dimx && need.min_y <= terminal.dimy) return element;
    }
    ui.setZoom(1);
    return make();
}

// + / = agrandan y - achica el zoom maximo del tablero.
bool adjust_zoom(const ftxui::Event& event, int& cap, std::string& notice) {
    if (!event.is_character()) return false;
    const std::string& key = event.character();
    if (key != "+" && key != "=" && key != "-") return false;
    cap = key == "-" ? std::max(1, cap - 1) : std::min(ConsoleUI::max_zoom, cap + 1);
    notice = "Zoom maximo del tablero: " + std::to_string(cap) +
             " (se ajusta solo al tamano de la ventana)";
    return true;
}

}  // namespace

GameApplication::GameApplication(AppOptions options)
    : options_(std::move(options)), ui_(options_.mode) {}

// ------------------------------------------------------------- Menu generico
std::optional<std::size_t> GameApplication::choose(const std::string& title,
                                                   const std::vector<MenuEntry>& entries,
                                                   const std::string& hint,
                                                   std::size_t initial) {
    MenuCursor cursor(entries.size());
    cursor.select(initial);
    std::optional<std::size_t> chosen;
    std::string notice;

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);
    auto document = ftxui::Renderer([&] {
        ftxui::Elements rows{ui_.menu(title, entries, cursor.selected(), hint)};
        if (!notice.empty()) rows.push_back(ui_.notice(notice) | ftxui::center);
        return ftxui::vbox(std::move(rows)) | ftxui::vcenter;
    });
    document = ftxui::CatchEvent(document, [&](const ftxui::Event& event) {
        if (!ConsoleUI::isKeyboardEvent(event)) return false;
        notice.clear();
        if (is_up(event)) {
            cursor.up();
        } else if (is_down(event)) {
            cursor.down();
        } else if (is_accept(event)) {
            if (entries[cursor.selected()].enabled) {
                chosen = cursor.selected();
                screen.Exit();
            } else {
                notice = "Esta opcion no esta disponible.";
            }
        } else if (is_back(event)) {
            screen.Exit();
        }
        return true;
    });
    screen.Loop(document);
    return chosen;
}

// ------------------------------------------------------------- Menu principal
void GameApplication::run() {
    const std::vector<MenuEntry> entries = {
        {"Jugar", "elige un nivel del lote preparado", true},
        {"Ver partida guardada", "reproduce una partida que jugaste y guardaste", true},
        {"Ver solucion del mapa", "la victoria comprobada por el generador", true},
        {"Como jugar", "reglas, controles y simbolos", true},
        {"Salir", "", true},
    };
    std::size_t last = 0;  // al volver, el cursor queda en la ultima opcion usada
    for (;;) {
        const auto option = choose("CIRCUITO DE ESCAPE", entries,
                                   "Flechas o W/S para elegir | Enter para aceptar | Q salir", last);
        if (!option || *option == 4) return;
        last = *option;
        if (*option == 0) {
            if (auto map = choose_level("Jugar: elige el nivel", false)) {
                NavigationEnvironment environment(to_scenario(*map));
                play(environment, true, &*map);
            }
        } else if (*option == 1) {
            watch_saved_game();
        } else if (*option == 2) {
            if (auto map = choose_level("Ver solucion del mapa: elige el nivel", true)) {
                replay(*map, map->winning_play,
                       "Solucion del mapa " + std::to_string(map->id) + " (" +
                           level_label(map->level) + ")",
                       EndReason::goalReached);
            }
        } else {
            how_to_play();
        }
    }
}

void GameApplication::watch_saved_game() {
    // Se relee en cada visita; la mas reciente primero (se muestran hasta 15).
    auto games = list_saved_games(options_.saves_dir);
    if (games.size() > 15) games.resize(15);
    std::vector<MenuEntry> entries;
    for (const SavedGameEntry& game : games) {
        const std::string label = game.record ? game.record->date : game.path.filename().string();
        entries.push_back({label, game.summary(), game.record.has_value()});
    }
    if (entries.empty()) {
        entries.push_back({"No hay partidas guardadas",
                           "al terminar una partida, pulsa G para guardarla", false});
    }
    const auto option = choose("Ver partida guardada", entries,
                               "Partidas en " + options_.saves_dir.string() +
                                   " | Enter para ver | Esc o Q volver");
    if (!option || games.empty()) return;
    const GameRecord& record = *games[*option].record;
    replay(record.map, record.actions, "Partida del " + record.date, record.result);
}

std::optional<Map> GameApplication::choose_level(const std::string& title, bool needWinningPlay) {
    // Se relee en cada visita: el lote pudo regenerarse con map_generator.
    const auto catalog = load_level_catalog(options_.maps_dir);
    std::vector<MenuEntry> entries;
    for (const LevelEntry& level : catalog) {
        const bool usable =
            level.available() && (!needWinningPlay || level.has_winning_play());
        std::string detail = level.summary();
        if (level.available() && needWinningPlay && !level.has_winning_play()) {
            detail = "sin partida ganadora guardada";
        }
        entries.push_back({level.label, detail, usable});
    }
    const auto option = choose(title, entries,
                               "Mapas en " + options_.maps_dir.string() +
                                   " | Enter para aceptar | Esc o Q volver");
    if (!option) return std::nullopt;
    return catalog[*option].map;
}

void GameApplication::how_to_play() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);
    auto document = ftxui::Renderer([&] { return ui_.how_to_play() | ftxui::vcenter; });
    document = ftxui::CatchEvent(document, [&](const ftxui::Event& event) {
        if (!ConsoleUI::isKeyboardEvent(event)) return false;
        if (is_accept(event) || is_back(event)) screen.Exit();
        return true;
    });
    screen.Loop(document);
}

// -------------------------------------------------------------------- Partida
void GameApplication::play(NavigationEnvironment& environment, bool fromMenu, const Map* map) {
    environment.reset(options_.seed);
    // Grabacion de la partida: una accion por turno jugado (H no cuenta).
    std::vector<Action> recorded;
    bool saved = false;

    std::unique_ptr<IController> controller;
    if (options_.controller) controller = makeController(*options_.controller, options_.seed);
    // El vigilante decide con BFS; el motor valida y aplica su movimiento.
    std::unique_ptr<IController> guard;
    if (environment.has_objectives()) guard = make_guard_controller(environment.grid());
    // Grafo del mapa para la ayuda H: los muros no cambian durante la partida.
    const Graph helpGraph(environment.grid(), environment.rules());
    EnergyHelp energyHelp;
    HelpHighlight highlight = HelpHighlight::route;

    std::vector<NavigationEvent> lastEvents;
    std::string notice;
    bool showHelp = false;
    const std::string saveKey = map ? "G guardar partida | " : "";
    const auto endKeys = [&] {
        return (saved ? std::string() : saveKey) +
               (fromMenu ? "R reintentar | M menu | Q menu" : "R reintentar | Q salir");
    };

    auto screen = fromMenu ? ftxui::ScreenInteractive::Fullscreen()
                           : ftxui::ScreenInteractive::TerminalOutput();
    screen.TrackMouse(false);

    const auto advance = [&](Action action) {
        if (environment.isFinished()) {
            notice = end_summary(environment) + ". " + endKeys();
            return;
        }
        const auto result = play_turn(environment, action, guard.get());
        recorded.push_back(action);
        lastEvents = result.events;
        notice = environment.isFinished() ? end_summary(environment) + ". " + endKeys() : "";
    };

    // Si ni con zoom 1 cabe la vista, se muestra un aviso y no se juega a ciegas
    // (lo decide el ultimo dibujo).
    bool tooSmall = false;
    const auto terminalTooSmall = [&] { return tooSmall; };

    // El tablero usa el zoom mas grande que cabe; en pantalla completa, centrado.
    int zoomCap = ConsoleUI::max_zoom;
    const auto place = [&](ftxui::Element element) {
        return fromMenu ? element | ftxui::center : element;
    };
    const std::string frameTitle =
        map ? "Circuito de Escape - " + level_label(map->level) + " (mapa " +
                  std::to_string(map->id) + ")"
            : std::string("Circuito de Escape");
    const std::string keys = std::string("WASD/flechas mover | E esperar | H ayuda | R reiniciar | "
                                         "+/- zoom | ") +
                             (fromMenu ? "M menu" : "Q salir");
    // Marco de tamano fijo: los textos que cambian no desplazan la pantalla.
    const auto gameView = [&] {
        return ui_.game_frame(environment, lastEvents, frameTitle, keys, notice);
    };

    auto document = ftxui::Renderer([&] {
        if (showHelp) {
            return place(fit_zoom(ui_, zoomCap, [&] {
                return ui_.help_screen(environment, energyHelp, highlight);
            }));
        }
        auto view = fit_zoom(ui_, zoomCap, gameView);
        view->ComputeRequirement();
        const ftxui::Dimensions need{view->requirement().min_x, view->requirement().min_y};
        const auto actual = ftxui::Terminal::Size();
        tooSmall = actual.dimx < need.dimx || actual.dimy < need.dimy;
        if (tooSmall) return ui_.too_small_message(need, actual);
        return place(std::move(view));
    });

    document = ftxui::CatchEvent(document, [&](const ftxui::Event& event) {
        if (!ConsoleUI::isKeyboardEvent(event)) return false;
        if (adjust_zoom(event, zoomCap, notice)) return true;
        if (showHelp && event.is_character()) {
            // Con la ayuda abierta, 1/2/3 eligen que camino resaltar (no juegan).
            const std::string& key = event.character();
            if (key == "1" || key == "2" || key == "3") {
                highlight = key == "1"   ? HelpHighlight::to_a
                            : key == "2" ? HelpHighlight::to_b
                                         : HelpHighlight::route;
                return true;
            }
        }
        if (!showHelp && is_key(event, 'r')) {
            // reset restaura el estado inicial: A y B desactivados, energia y turno 0.
            environment.reset(options_.seed);
            lastEvents.clear();
            recorded.clear();  // la grabacion empieza de nuevo
            saved = false;
            notice = "Partida reiniciada.";
            return true;
        }
        if (!showHelp && map && is_key(event, 'g')) {
            if (!environment.isFinished()) {
                notice = "La partida se puede guardar al terminar.";
            } else if (saved) {
                notice = "Esta partida ya fue guardada.";
            } else {
                try {
                    GameRecord record{*map, recorded, environment.endReason(), environment.turn(),
                                      environment.energy(), current_date()};
                    const auto path = save_record(std::move(record), options_.saves_dir);
                    saved = true;
                    notice = "Partida guardada en " + path.string() + ". " + endKeys();
                } catch (const std::exception& error) {
                    notice = std::string("No se pudo guardar: ") + error.what();
                }
            }
            return true;
        }
        if (!showHelp && fromMenu && is_key(event, 'm')) {
            screen.Exit();
            return true;
        }

        const auto command = ui_.translate(event);
        if (!command) {
            notice = "Comando desconocido. WASD mover, E esperar, H ayuda, R reiniciar, Q salir.";
            return true;
        }
        switch (resolve_command(*command, showHelp, event == ftxui::Event::Escape)) {
            case UiOutcome::quit:
                screen.Exit();
                return true;
            case UiOutcome::open_help:
                // Se calcula al abrir H, desde la posicion, energia e interruptores
                // actuales. Mientras esta abierta no pasa ningun turno.
                energyHelp = compute_energy_help(environment, helpGraph);
                highlight = HelpHighlight::route;
                showHelp = true;
                return true;
            case UiOutcome::close_help:
                // H, Esc o una tecla de juego solo cierran la ayuda: no pasa ningun turno.
                showHelp = false;
                return true;
            case UiOutcome::play_action:
                break;
        }

        if (terminalTooSmall()) return true;  // sin ver el tablero no se juega
        if (controller) {
            const auto state = environment.state();
            advance(environment.isFinished() ? Action::wait
                                             : controller->selectAction(state, state.availableActions));
            return true;
        }
        advance(std::get<Action>(*command));
        return true;
    });

    screen.Loop(document);

    if (!fromMenu) {
        const auto final_ = environment.state();
        std::cout << "Resultado: " << toString(environment.endReason()) << '\n'
                  << "Turnos: " << final_.turn << '\n'
                  << "Energia restante: " << final_.energy << '/' << final_.maximumEnergy << '\n'
                  << "Recursos: " << final_.collectedResources << '/'
                  << environment.totalResources() << '\n'
                  << "Puntaje: " << final_.score << '\n';
    }
}

// ----------------------------------------------- Reproduccion de acciones
void GameApplication::replay(const Map& map, std::vector<Action> actions,
                             const std::string& title, std::optional<EndReason> expected) {
    ReplaySession session(map, std::move(actions));
    std::atomic<bool> autoplay{false};

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);

    const auto status = [&] {
        std::string text = title + ": turno " + std::to_string(session.played()) + "/" +
                           std::to_string(session.total());
        if (!session.finished()) {
            text += " | siguiente: " + describe_action(session.next_action());
        } else {
            text += " | " + end_summary(session.environment());
            // Comprobacion: la reproduccion termina como se guardo.
            if (expected) {
                text += *expected == session.environment().endReason()
                            ? " (coincide con lo guardado)"
                            : " (NO coincide con lo guardado)";
            }
        }
        text += autoplay ? " | AUTOMATICO" : "";
        return text;
    };

    int zoomCap = ConsoleUI::max_zoom;
    std::string zoomNotice;
    auto document = ftxui::Renderer([&] {
        auto view = fit_zoom(ui_, zoomCap, [&] {
            return ui_.game_frame(session.environment(), session.last_events(), title,
                                  "Espacio/Enter avanza | A automatico | R desde el inicio | "
                                  "+/- zoom | Q volver",
                                  zoomNotice.empty() ? status() : zoomNotice);
        });
        view->ComputeRequirement();
        const ftxui::Dimensions need{view->requirement().min_x, view->requirement().min_y};
        const auto actual = ftxui::Terminal::Size();
        if (actual.dimx < need.dimx || actual.dimy < need.dimy) {
            return ui_.too_small_message(need, actual);
        }
        return view | ftxui::center;
    });
    document = ftxui::CatchEvent(document, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) {  // pulso del modo automatico
            if (autoplay && !session.step()) autoplay = false;
            return true;
        }
        if (!ConsoleUI::isKeyboardEvent(event)) return false;
        zoomNotice.clear();
        if (adjust_zoom(event, zoomCap, zoomNotice)) return true;
        if (is_back(event)) {
            screen.Exit();
        } else if (is_accept(event) || event == ftxui::Event::ArrowRight) {
            autoplay = false;
            (void)session.step();
        } else if (is_key(event, 'a')) {
            autoplay = !autoplay && !session.finished();
        } else if (is_key(event, 'r')) {
            autoplay = false;
            session.restart();
        }
        return true;
    });

    // Pulso del modo automatico: un hilo que solo despierta la interfaz.
    // El motor se usa siempre desde el hilo de la interfaz.
    std::jthread ticker([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (autoplay) screen.PostEvent(ftxui::Event::Custom);
        }
    });
    screen.Loop(document);
    autoplay = false;
    ticker.request_stop();
    ticker.join();
}

}  // namespace circuit_escape
