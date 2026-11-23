#include "../include/circuit_escape/console_ui.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>
#include <variant>

namespace circuit_escape {

namespace {

constexpr std::array<std::string_view, 10> kKeycapDigits{
    "0\xEF\xB8\x8F\xE2\x83\xA3", "1\xEF\xB8\x8F\xE2\x83\xA3",
    "2\xEF\xB8\x8F\xE2\x83\xA3", "3\xEF\xB8\x8F\xE2\x83\xA3",
    "4\xEF\xB8\x8F\xE2\x83\xA3", "5\xEF\xB8\x8F\xE2\x83\xA3",
    "6\xEF\xB8\x8F\xE2\x83\xA3", "7\xEF\xB8\x8F\xE2\x83\xA3",
    "8\xEF\xB8\x8F\xE2\x83\xA3", "9\xEF\xB8\x8F\xE2\x83\xA3"};

constexpr std::string_view kEmojiEmpty = "\xE2\xAC\x9C";
constexpr std::string_view kEmojiWall = "\xF0\x9F\xA7\xB1";
constexpr std::string_view kEmojiRough = "\xF0\x9F\xAA\xA8";
constexpr std::string_view kEmojiResource = "\xF0\x9F\x92\x8E";
constexpr std::string_view kEmojiBattery = "\xF0\x9F\x94\x8B";
constexpr std::string_view kEmojiTrap = "\xF0\x9F\x92\x80";
constexpr std::string_view kEmojiExit = "\xF0\x9F\x9A\xAA";
constexpr std::string_view kEmojiAgent = "\xF0\x9F\xA4\x96";
constexpr std::string_view kEmojiGuard = "\xF0\x9F\x91\xBE";
constexpr std::string_view kEmojiSwitchOff = "\xF0\x9F\x94\xB4";
constexpr std::string_view kEmojiSwitchOn = "\xF0\x9F\x9F\xA2";

std::optional<UiCommand> fromLetter(char letter) {
    switch (std::tolower(static_cast<unsigned char>(letter))) {
        case 'w': return UiCommand{Action::up};
        case 's': return UiCommand{Action::down};
        case 'a': return UiCommand{Action::left};
        case 'd': return UiCommand{Action::right};
        case 'e': return UiCommand{Action::wait};
        case 'h': return UiCommand{HelpCommand{}};
        case 'q': return UiCommand{QuitCommand{}};
        default: return std::nullopt;
    }
}

}

std::optional<RenderMode> parseRenderMode(std::string_view name) noexcept {
    if (name == "emoji") return RenderMode::emoji;
    if (name == "ascii") return RenderMode::ascii;
    return std::nullopt;
}

std::string_view toString(RenderMode mode) noexcept {
    return mode == RenderMode::ascii ? "ascii" : "emoji";
}

ftxui::Element cellBox(std::string label) {
    return ftxui::text(std::move(label)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2);
}

UiOutcome resolve_command(const UiCommand& command, bool help_open, bool escape_pressed) noexcept {
    return std::visit(
        Overloaded{
            [&](QuitCommand) {
                return (help_open && escape_pressed) ? UiOutcome::close_help : UiOutcome::quit;
            },
            [&](HelpCommand) { return help_open ? UiOutcome::close_help : UiOutcome::open_help; },
            [&](Action) { return help_open ? UiOutcome::close_help : UiOutcome::play_action; }},
        command);
}

std::optional<UiCommand> ConsoleUI::translate(const ftxui::Event& event) const {
    if (event == ftxui::Event::ArrowUp) return UiCommand{Action::up};
    if (event == ftxui::Event::ArrowDown) return UiCommand{Action::down};
    if (event == ftxui::Event::ArrowLeft) return UiCommand{Action::left};
    if (event == ftxui::Event::ArrowRight) return UiCommand{Action::right};
    if (event == ftxui::Event::Escape) return UiCommand{QuitCommand{}};

    if (event.is_character()) {
        const std::string& input = event.character();
        if (input.size() == 1) return fromLetter(input.front());
    }
    return std::nullopt;
}

bool ConsoleUI::isKeyboardEvent(const ftxui::Event& event) {
    return !event.is_mouse() && !event.is_cursor_position() &&
           !event.is_cursor_shape() && event != ftxui::Event::Custom;
}

std::string ConsoleUI::emptyGlyph() const {
    return mode_ == RenderMode::ascii ? "." : std::string(kEmojiEmpty);
}

std::string ConsoleUI::guardGlyph() const {
    return mode_ == RenderMode::ascii ? "G" : std::string(kEmojiGuard);
}

std::string ConsoleUI::switchGlyph(bool active) const {
    if (mode_ == RenderMode::ascii) return active ? "+" : "!";
    return std::string(active ? kEmojiSwitchOn : kEmojiSwitchOff);
}

std::string ConsoleUI::glyphAt(const NavigationEnvironment& environment, Position position) const {
    if (environment.has_objectives()) {
        if (position == environment.guard_position()) return guardGlyph();
        if (position == environment.player_position()) return agentGlyph();
        for (const SwitchId id : {SwitchId::a, SwitchId::b}) {
            if (position == environment.switch_position(id)) {
                return switchGlyph(environment.is_switch_active(id));
            }
        }
    } else if (position == environment.player_position()) {
        return agentGlyph();
    }
    return glyphFor(environment.grid().at(position));
}

std::string ConsoleUI::objectivesStatus(const NavigationEnvironment& environment) const {
    if (!environment.has_objectives()) return "";
    const auto state = [&](SwitchId id) {
        return environment.is_switch_active(id) ? "activado" : "pendiente";
    };
    std::string text = std::string("A ") + state(SwitchId::a) + " | B " + state(SwitchId::b);
    if (!environment.isFinished()) {
        // Tras tu proximo movimiento el turno sera el numero turn()+1.
        const bool guardMoves = (environment.turn() + 1) % 2 == 0;
        text += guardMoves ? " | Vigilante: se mueve tras tu accion"
                           : " | Vigilante: quieto este turno";
    }
    return text;
}

std::string ConsoleUI::agentGlyph() const {
    return mode_ == RenderMode::ascii ? "@" : std::string(kEmojiAgent);
}

std::string ConsoleUI::glyphFor(const Cell& cell) const {
    const bool ascii = mode_ == RenderMode::ascii;
    return std::visit(
        Overloaded{
            [&](const Empty&) { return emptyGlyph(); },
            [&](const Wall&) { return ascii ? std::string("#") : std::string(kEmojiWall); },
            [&](const RoughTerrain&) {
                return ascii ? std::string("~") : std::string(kEmojiRough);
            },
            [&](const ResourceCell<int>& resource) {
                if (resource.collected) return emptyGlyph();
                return ascii ? std::string("R") : std::string(kEmojiResource);
            },
            [&](const Battery& battery) {
                if (battery.consumed) return emptyGlyph();
                return ascii ? std::string("B") : std::string(kEmojiBattery);
            },
            [&](const Trap&) { return ascii ? std::string("T") : std::string(kEmojiTrap); },
            [&](const Exit&) { return ascii ? std::string("S") : std::string(kEmojiExit); }},
        cell);
}

std::string ConsoleUI::coordinateLabel(std::size_t index) const {
    const std::size_t digit = index % 10;
    if (mode_ == RenderMode::ascii) return std::to_string(digit);
    return std::string(kKeycapDigits[digit]);
}

ftxui::Element ConsoleUI::statusBar(const Observation& state, std::size_t totalResources,
                                   std::size_t turnLimit) const {
    std::string text = "Turno " + std::to_string(state.turn) + "/" + std::to_string(turnLimit);
    text += " | Energia " + std::to_string(state.energy) + "/" +
            std::to_string(state.maximumEnergy);
    text += " | Puntaje " + std::to_string(state.score);
    text += " | Recursos " + std::to_string(state.collectedResources) + "/" +
            std::to_string(totalResources);
    return ftxui::text(text) | ftxui::bold;
}

ftxui::Element ConsoleUI::tile(std::string label, bool wall) const {
    if (zoom_ == 1) return cellBox(std::move(label));
    // Casilla ampliada: el simbolo centrado en un bloque de 2z x z.
    auto element = ftxui::text(std::move(label)) | ftxui::center |
                   ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2 * zoom_) |
                   ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, zoom_);
    if (wall) element = element | ftxui::bgcolor(ftxui::Color::GrayDark);
    return element;
}

ftxui::Element ConsoleUI::horizontalRuler(std::size_t columns) const {
    ftxui::Elements labels;
    labels.push_back(cellBox(emptyGlyph()) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2 * zoom_));
    for (std::size_t column = 0; column < columns; ++column) {
        labels.push_back(cellBox(coordinateLabel(column)) | ftxui::hcenter |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2 * zoom_));
    }
    return ftxui::hbox(std::move(labels));
}

namespace {

std::string coordinates(Position position) {
    return "(" + std::to_string(position.row) + "," + std::to_string(position.column) + ")";
}

std::string directionName(Action action) {
    switch (action) {
        case Action::up: return "arriba";
        case Action::down: return "abajo";
        case Action::left: return "izquierda";
        case Action::right: return "derecha";
        case Action::wait: return "esperar";
    }
    return "?";
}

int relevance(const NavigationEvent& event) {
    return std::visit(
        Overloaded{
            [](const CapturedEvent&) { return 8; },
            [](const GoalReachedEvent&) { return 6; },
            [](const SwitchActivatedEvent&) { return 7; },
            [](const TrapTriggeredEvent&) { return 5; },
            [](const ResourceCollectedEvent&) { return 4; },
            [](const EnergyChangedEvent& e) { return e.current > e.previous ? 3 : 0; },
            [](const MovementRejectedEvent&) { return 2; },
            [](const MovedEvent&) { return 1; },
            [](const GuardMovedEvent&) { return 1; }},
        event);
}

}

std::string ConsoleUI::lastEventText(std::span<const NavigationEvent> stepEvents) const {
    if (stepEvents.empty()) return "sin eventos";

    const auto headline = std::max_element(
        stepEvents.begin(), stepEvents.end(),
        [](const NavigationEvent& a, const NavigationEvent& b) {
            return relevance(a) < relevance(b);
        });

    return std::visit(
        Overloaded{
            [&](const GoalReachedEvent& e) {
                return glyphFor(Cell{Exit{}}) + " salida alcanzada en " + coordinates(e.at);
            },
            [&](const TrapTriggeredEvent& e) {
                return glyphFor(Cell{Trap{}}) + " trampa en " + coordinates(e.at);
            },
            [&](const ResourceCollectedEvent& e) {
                return glyphFor(Cell{ResourceCell<int>{}}) + " +" + std::to_string(e.points) +
                       " en " + coordinates(e.at);
            },
            [&](const EnergyChangedEvent& e) {
                if (e.current > e.previous) {
                    return glyphFor(Cell{Battery{}}) + " +" +
                           std::to_string(e.current - e.previous) + " energia";
                }
                return "energia " + std::to_string(e.previous) + " -> " +
                       std::to_string(e.current);
            },
            [&](const MovementRejectedEvent& e) {
                return "movimiento bloqueado hacia " + directionName(e.action);
            },
            [&](const MovedEvent& e) { return agentGlyph() + " en " + coordinates(e.to); },
            [&](const GuardMovedEvent& e) { return "vigilante en " + coordinates(e.to); },
            [&](const SwitchActivatedEvent& e) {
                return std::string("interruptor ") + (e.id == SwitchId::a ? "A" : "B") +
                       " activado en " + coordinates(e.at);
            },
            [&](const CapturedEvent& e) { return "capturado en " + coordinates(e.at); }},
        *headline);
}

ftxui::Element ConsoleUI::footer(std::span<const NavigationEvent> recentEvents,
                                 EndReason reason) const {
    const std::string separator = mode_ == RenderMode::ascii ? " - " : " \xC2\xB7 ";
    std::string shortHelp;
    if (reason != EndReason::none) {
        shortHelp = "Fin: " + toString(reason) + separator + "Q salir";
    } else {
        shortHelp = "WASD mover" + separator + "E esperar" + separator + "H ayuda" +
                    separator + "Q salir";
    }
    return ftxui::text(lastEventText(recentEvents) + " | " + shortHelp) | ftxui::dim;
}

ftxui::Element ConsoleUI::notice(const std::string& text) const {
    return ftxui::text(text) | ftxui::inverted;
}

ftxui::Element ConsoleUI::board(const NavigationEnvironment& environment,
                                std::span<const Position> highlight) const {
    const auto& grid = environment.grid();
    std::vector<bool> marked(grid.rows() * grid.columns(), false);
    for (Position p : highlight) {
        if (grid.contains(p)) marked[p.row * grid.columns() + p.column] = true;
    }

    ftxui::Elements lines;
    lines.push_back(horizontalRuler(grid.columns()));
    for (std::size_t row = 0; row < grid.rows(); ++row) {
        ftxui::Elements cells;
        cells.push_back(cellBox(coordinateLabel(row)) | ftxui::vcenter |
                        ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2 * zoom_) |
                        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, zoom_));
        for (std::size_t column = 0; column < grid.columns(); ++column) {
            const Position position{row, column};
            auto cell = tile(glyphAt(environment, position),
                             std::holds_alternative<Wall>(grid.at(position)));
            if (marked[row * grid.columns() + column]) {
                cell = cell | ftxui::bgcolor(ftxui::Color::Blue) | ftxui::bold;
            }
            cells.push_back(std::move(cell));
        }
        lines.push_back(ftxui::hbox(std::move(cells)));
    }
    return ftxui::vbox(std::move(lines));
}

ftxui::Element ConsoleUI::game_frame(const NavigationEnvironment& environment,
                                    std::span<const NavigationEvent> recentEvents,
                                    const std::string& title, const std::string& keys,
                                    const std::string& notice,
                                    std::span<const Position> highlight) const {
    auto boardView = board(environment, highlight);
    boardView->ComputeRequirement();
    const int width = std::max(boardView->requirement().min_x, frame_min_width);

    // Cada linea ocupa exactamente `width` columnas (se rellena o se recorta).
    const auto line = [width](ftxui::Element element) {
        return element | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
    };
    const Observation state = environment.state();
    std::string status = "Turno " + std::to_string(state.turn) + "/" +
                         std::to_string(environment.rules().turnLimit) + " | Energia " +
                         std::to_string(state.energy) + "/" + std::to_string(state.maximumEnergy) +
                         " | Puntaje " + std::to_string(state.score) + " | Recursos " +
                         std::to_string(state.collectedResources) + "/" +
                         std::to_string(environment.totalResources());

    ftxui::Elements rows;
    rows.push_back(line(ftxui::text(status) | ftxui::bold));
    // La linea de objetivos existe siempre (vacia en mapas sin interruptores).
    rows.push_back(line(ftxui::text(objectivesStatus(environment)) | ftxui::bold));
    rows.push_back(ftxui::separator());
    rows.push_back(line(std::move(boardView) | ftxui::hcenter));
    rows.push_back(ftxui::separator());
    std::string last = "Ultimo: " + lastEventText(recentEvents);
    if (environment.endReason() != EndReason::none) {
        last += " | Fin: " + toString(environment.endReason());
    }
    rows.push_back(line(ftxui::text(last)));
    rows.push_back(line(ftxui::text(keys) | ftxui::dim));
    // Aviso: dos lineas reservadas aunque este vacio.
    auto message = notice.empty() ? ftxui::text("") : ftxui::paragraph(notice) | ftxui::inverted;
    rows.push_back(line(std::move(message) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 2)));

    return ftxui::window(ftxui::text(" " + title + " ") | ftxui::bold, ftxui::vbox(std::move(rows)));
}

ftxui::Element ConsoleUI::help_screen(const NavigationEnvironment& environment,
                                      const EnergyHelp& energyHelp,
                                      HelpHighlight highlight) const {
    ftxui::Elements panel;
    panel.push_back(ftxui::text("Ayuda de energia (Dijkstra) - turno " +
                                std::to_string(energyHelp.turn)) |
                    ftxui::bold);
    panel.push_back(ftxui::text(""));
    for (const std::string& line : format_energy_help(energyHelp)) {
        panel.push_back(ftxui::text(line));
    }
    panel.push_back(ftxui::text(""));
    panel.push_back(ftxui::text("No anticipa al vigilante ni las recargas: los desvios y") |
                    ftxui::dim);
    panel.push_back(ftxui::text("esperas gastan mas. No garantiza un escape seguro.") |
                    ftxui::dim);
    panel.push_back(ftxui::text(""));

    const char* names[] = {"recorrido completo", "camino hacia A", "camino hacia B"};
    const auto index = static_cast<std::size_t>(highlight);
    std::string shown = std::string("Resaltado: ") + names[index];
    if (energyHelp.path(highlight).empty()) shown += " (no aplica)";
    panel.push_back(ftxui::text(shown) | ftxui::color(ftxui::Color::Blue) | ftxui::bold);
    panel.push_back(ftxui::text("1 hacia A | 2 hacia B | 3 recorrido | H o Esc volver") |
                    ftxui::dim);

    return ftxui::vbox({
        ftxui::hbox({ftxui::vbox(std::move(panel)) | ftxui::border, help()}),
        board(environment, energyHelp.path(highlight)),
    });
}

ftxui::Element ConsoleUI::menu(const std::string& title, std::span<const MenuEntry> entries,
                               std::size_t selected, const std::string& hint) const {
    const std::string marker = mode_ == RenderMode::ascii ? "> " : "\xE2\x96\xB6 ";
    ftxui::Elements rows;
    rows.push_back(ftxui::text(title) | ftxui::bold | ftxui::center);
    rows.push_back(ftxui::separator());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const MenuEntry& entry = entries[i];
        const bool current = i == selected;
        auto label = ftxui::text((current ? marker : std::string("  ")) + entry.label);
        if (current) label = label | ftxui::bold | ftxui::inverted;
        if (!entry.enabled) label = label | ftxui::dim;
        ftxui::Elements line{label};
        if (!entry.detail.empty()) {
            line.push_back(ftxui::text("   " + entry.detail) | ftxui::dim);
        }
        rows.push_back(ftxui::hbox(std::move(line)));
    }
    rows.push_back(ftxui::separator());
    rows.push_back(ftxui::text(hint) | ftxui::dim);
    return ftxui::vbox(std::move(rows)) | ftxui::border | ftxui::center;
}

ftxui::Element ConsoleUI::how_to_play() const {
    const auto line = [](const std::string& text) { return ftxui::text(text); };
    auto rules = ftxui::vbox({
        ftxui::text("Como jugar") | ftxui::bold,
        line(""),
        line("Objetivo: activa los interruptores A y B, en cualquier orden, y"),
        line("despues llega a la salida con energia restante, sin ser capturado"),
        line("y dentro del limite de turnos."),
        line(""),
        line("- Pisar un interruptor lo activa y queda activado."),
        line("- La salida se puede pisar antes, pero solo sirve con A y B activos."),
        line("- Cada movimiento cuesta la energia de la casilla a la que entras;"),
        line("  el terreno elevado cuesta mas. Esperar o chocar cuesta 1."),
        line("- Las baterias recargan energia; las trampas la quitan."),
        line("- Los recursos dan puntos (no son necesarios para ganar)."),
        line("- El vigilante te persigue por el camino mas corto (BFS)."),
        line("  Se mueve en los turnos PARES. Si te alcanza, pierdes, aunque"),
        line("  estes en la salida."),
        line("- Con energia 0 pierdes; llegar a la salida con 0 no gana."),
        line(""),
        line("Durante la partida, H abre la ayuda de energia (Dijkstra):"),
        line("gasto hacia A, B y la salida, recorrido sugerido y saldo."),
        line("Mientras esta abierta no pasa ningun turno."),
        line(""),
        line("R reinicia la partida | M vuelve al menu"),
        line("Al terminar, G guarda la partida para verla en el menu."),
        line("El tablero se agranda solo segun la ventana; + y - lo ajustan."),
        line("Para agrandar tambien el texto, use el zoom de la terminal (Ctrl +)."),
        line(""),
        ftxui::text("Enter, Esc o Q para volver") | ftxui::dim,
    }) | ftxui::border;
    return ftxui::hbox({rules, help()}) | ftxui::center;
}

ftxui::Element ConsoleUI::help() const {
    return ftxui::vbox({
               ftxui::text("Circuito de Escape - ayuda") | ftxui::bold,
               ftxui::text(""),
               ftxui::text("W / flecha arriba    mover arriba"),
               ftxui::text("S / flecha abajo     mover abajo"),
               ftxui::text("A / flecha izquierda mover a la izquierda"),
               ftxui::text("D / flecha derecha   mover a la derecha"),
               ftxui::text("E                    esperar un turno"),
               ftxui::text("H                    mostrar u ocultar esta ayuda"),
               ftxui::text("Esc                  cerrar la ayuda (con la ayuda cerrada, abandonar)"),
               ftxui::text("Q                    abandonar la partida"),
               ftxui::text("Con la ayuda abierta no pasa ningun turno."),
               ftxui::text("1 / 2 / 3            resaltar camino a A / a B / recorrido"),
               ftxui::text("R                    reiniciar la partida"),
               ftxui::text("G                    guardar la partida (al terminar)"),
               ftxui::text("+ / -                agrandar / achicar el tablero"),
               ftxui::text(""),
               ftxui::text("Leyenda") | ftxui::bold,
               ftxui::hbox({cellBox(agentGlyph()), ftxui::text(" agente")}),
               ftxui::hbox({cellBox(emptyGlyph()), ftxui::text(" espacio libre")}),
               ftxui::hbox({cellBox(glyphFor(Cell{Wall{}})), ftxui::text(" muro")}),
               ftxui::hbox({cellBox(glyphFor(Cell{RoughTerrain{}})),
                            ftxui::text(" terreno de costo elevado")}),
               ftxui::hbox({cellBox(glyphFor(Cell{ResourceCell<int>{}})),
                            ftxui::text(" recurso")}),
               ftxui::hbox({cellBox(glyphFor(Cell{Battery{}})), ftxui::text(" bateria")}),
               ftxui::hbox({cellBox(glyphFor(Cell{Trap{}})), ftxui::text(" trampa")}),
               ftxui::hbox({cellBox(glyphFor(Cell{Exit{}})), ftxui::text(" salida")}),
               ftxui::hbox({cellBox(guardGlyph()), ftxui::text(" vigilante")}),
               ftxui::hbox({cellBox(switchGlyph(false)), ftxui::text(" interruptor pendiente")}),
               ftxui::hbox({cellBox(switchGlyph(true)), ftxui::text(" interruptor activado")}),
           }) |
           ftxui::border;
}

ftxui::Dimensions ConsoleUI::required_size(const NavigationEnvironment& environment) const {
    const std::vector<NavigationEvent> noEvents;
    auto document = render(environment, noEvents);
    document->ComputeRequirement();
    return ftxui::Dimensions{document->requirement().min_x, document->requirement().min_y};
}

ftxui::Element ConsoleUI::too_small_message(ftxui::Dimensions required,
                                            ftxui::Dimensions actual) const {
    const auto size = [](ftxui::Dimensions d) {
        return std::to_string(d.dimx) + "x" + std::to_string(d.dimy);
    };
    return ftxui::vbox({
               ftxui::text("Terminal demasiado pequena para este tablero.") | ftxui::bold,
               ftxui::text("Se necesita al menos " + size(required) + " (columnas x filas)."),
               ftxui::text("Tamano actual: " + size(actual) + "."),
               ftxui::text("Amplie la ventana o elija un tablero menor."),
               ftxui::text("Q para salir."),
           }) |
           ftxui::border;
}

}