#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "./cells.hpp"
#include "./environment.hpp"
#include "./events.hpp"
#include "./position.hpp"
#include "./ui/energy_help.hpp"

namespace circuit_escape {

struct QuitCommand {};
struct HelpCommand {};

using UiCommand = std::variant<Action, QuitCommand, HelpCommand>;

enum class UiOutcome { quit, open_help, close_help, play_action };
[[nodiscard]] UiOutcome resolve_command(const UiCommand& command, bool help_open,
                                            bool escape_pressed) noexcept;

enum class RenderMode { emoji, ascii };

// Opcion de un menu vertical.
struct MenuEntry {
    std::string label;
    std::string detail;   // texto secundario (p. ej. resumen del nivel)
    bool enabled{true};   // deshabilitada: se muestra atenuada y no se elige
};

[[nodiscard]] std::optional<RenderMode> parseRenderMode(std::string_view name) noexcept;
[[nodiscard]] std::string_view toString(RenderMode mode) noexcept;

[[nodiscard]] ftxui::Element cellBox(std::string label);

class ConsoleUI {
public:
    explicit ConsoleUI(RenderMode mode = RenderMode::emoji) : mode_(mode) {}

    [[nodiscard]] RenderMode mode() const noexcept { return mode_; }
    void setMode(RenderMode mode) noexcept { mode_ = mode; }

    // Zoom del tablero: cada casilla ocupa (2*zoom) columnas x zoom filas.
    // Con zoom 1 se ve como siempre; con zoom >= 2 los muros se rellenan.
    static constexpr int max_zoom = 4;
    [[nodiscard]] int zoom() const noexcept { return zoom_; }
    void setZoom(int zoom) noexcept { zoom_ = zoom < 1 ? 1 : (zoom > max_zoom ? max_zoom : zoom); }

    [[nodiscard]] std::optional<UiCommand> translate(const ftxui::Event& event) const;

    [[nodiscard]] std::string glyphFor(const Cell& cell) const;
    [[nodiscard]] std::string agentGlyph() const;
    [[nodiscard]] std::string emptyGlyph() const;
    [[nodiscard]] std::string guardGlyph() const;
    [[nodiscard]] std::string switchGlyph(bool active) const;
    [[nodiscard]] std::string coordinateLabel(std::size_t index) const;
    
    [[nodiscard]] static bool isKeyboardEvent(const ftxui::Event& event);

    [[nodiscard]] ftxui::Element help() const;
    [[nodiscard]] ftxui::Dimensions required_size(const NavigationEnvironment& environment) const;
    [[nodiscard]] ftxui::Element too_small_message(ftxui::Dimensions required, ftxui::Dimensions actual) const;
    [[nodiscard]] ftxui::Element notice(const std::string& text) const;

    [[nodiscard]] std::string lastEventText(std::span<const NavigationEvent> stepEvents) const;

    [[nodiscard]] ftxui::Element render(
        const NavigationEnvironment& environment,
        std::span<const NavigationEvent> recentEvents) const {
        const Observation state = environment.state();

        ftxui::Elements lines;
        lines.push_back(statusBar(state, environment.totalResources(), environment.rules().turnLimit));
        if (const std::string objectives = objectivesStatus(environment); !objectives.empty()) {
            lines.push_back(ftxui::text(objectives) | ftxui::bold);
        }
        lines.push_back(board(environment, {}));
        lines.push_back(footer(recentEvents, environment.endReason()));
        return ftxui::vbox(std::move(lines));
    }

    // Vista de la partida dentro de un marco de tamano FIJO: el ancho y el alto
    // dependen solo del tablero y del zoom, nunca del largo de los textos, asi
    // que al centrarla en la pantalla no se desplaza al cambiar los mensajes.
    //   titulo | estado | objetivos | tablero centrado | ultimo evento | teclas | aviso (2 lineas)
    static constexpr int frame_min_width = 74;
    [[nodiscard]] ftxui::Element game_frame(const NavigationEnvironment& environment,
                                            std::span<const NavigationEvent> recentEvents,
                                            const std::string& title, const std::string& keys,
                                            const std::string& notice,
                                            std::span<const Position> highlight = {}) const;

    // Tablero con reglas de coordenadas; las casillas de `highlight` se resaltan.
    [[nodiscard]] ftxui::Element board(const NavigationEnvironment& environment,
                                       std::span<const Position> highlight) const;

    // Menu vertical con titulo; la opcion `selected` aparece marcada.
    [[nodiscard]] ftxui::Element menu(const std::string& title,
                                      std::span<const MenuEntry> entries, std::size_t selected,
                                      const std::string& hint) const;

    // Reglas del juego, controles y leyenda (opcion "Como jugar").
    [[nodiscard]] ftxui::Element how_to_play() const;

    // Pantalla H (seccion 3.3): ayuda de energia ya calculada, tablero con el
    // camino elegido resaltado, controles y leyenda. No juega ningun turno.
    [[nodiscard]] ftxui::Element help_screen(const NavigationEnvironment& environment,
                                             const EnergyHelp& energyHelp,
                                             HelpHighlight highlight) const;

private:
    [[nodiscard]] ftxui::Element statusBar(const Observation& state,
                                          std::size_t totalResources,
                                          std::size_t turnLimit) const;
    [[nodiscard]] std::string glyphAt(const NavigationEnvironment& environment, Position position) const;
    [[nodiscard]] std::string objectivesStatus(const NavigationEnvironment& environment) const;
    [[nodiscard]] ftxui::Element horizontalRuler(std::size_t columns) const;
    [[nodiscard]] ftxui::Element footer(std::span<const NavigationEvent> recentEvents, EndReason reason) const;

    // Casilla del tablero con el tamano del zoom actual.
    [[nodiscard]] ftxui::Element tile(std::string label, bool wall = false) const;

    RenderMode mode_;
    int zoom_{1};
};

}  // namespace circuit_escape
