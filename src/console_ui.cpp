#include "../include/circuit_escape/console_ui.hpp"
#include <algorithm>
#include <array>
#include <cctype>
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

ftxui::Element ConsoleUI::horizontalRuler(std::size_t columns) const {
    ftxui::Elements labels;
    labels.push_back(cellBox(emptyGlyph()));
    for (std::size_t column = 0; column < columns; ++column) {
        labels.push_back(cellBox(coordinateLabel(column)));
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
            [](const GoalReachedEvent&) { return 6; },
            [](const TrapTriggeredEvent&) { return 5; },
            [](const ResourceCollectedEvent&) { return 4; },
            [](const EnergyChangedEvent& e) { return e.current > e.previous ? 3 : 0; },
            [](const MovementRejectedEvent&) { return 2; },
            [](const MovedEvent&) { return 1; }},
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
            [&](const MovedEvent& e) { return agentGlyph() + " en " + coordinates(e.to); }},
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
               ftxui::text("Q / Esc              abandonar la partida"),
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
           }) |
           ftxui::border;
}

}
