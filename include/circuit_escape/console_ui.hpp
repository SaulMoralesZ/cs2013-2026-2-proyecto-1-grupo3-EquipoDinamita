#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "./cells.hpp"
#include "./environment.hpp"
#include "./events.hpp"
#include "./position.hpp"

namespace circuit_escape {

struct QuitCommand {};
struct HelpCommand {};

using UiCommand = std::variant<Action, QuitCommand, HelpCommand>;

enum class RenderMode { emoji, ascii };

[[nodiscard]] std::optional<RenderMode> parseRenderMode(std::string_view name) noexcept;
[[nodiscard]] std::string_view toString(RenderMode mode) noexcept;

[[nodiscard]] ftxui::Element cellBox(std::string label);

class ConsoleUI {
public:
    explicit ConsoleUI(RenderMode mode = RenderMode::emoji) : mode_(mode) {}

    [[nodiscard]] RenderMode mode() const noexcept { return mode_; }
    void setMode(RenderMode mode) noexcept { mode_ = mode; }

    [[nodiscard]] std::optional<UiCommand> translate(const ftxui::Event& event) const;

    [[nodiscard]] std::string glyphFor(const Cell& cell) const;
    [[nodiscard]] std::string agentGlyph() const;
    [[nodiscard]] std::string emptyGlyph() const;
    [[nodiscard]] std::string coordinateLabel(std::size_t index) const;
    
    [[nodiscard]] static bool isKeyboardEvent(const ftxui::Event& event);

    [[nodiscard]] ftxui::Element help() const;
    [[nodiscard]] ftxui::Element notice(const std::string& text) const;

    template<std::size_t Rows, std::size_t Columns>
    [[nodiscard]] ftxui::Element render(
        const NavigationEnvironment<Rows, Columns>& environment,
        std::span<const NavigationEvent> recentEvents) const {
        const Observation state = environment.state();

        ftxui::Elements lines;
        lines.push_back(statusBar(state, environment.totalResources(),
                                  environment.rules().turnLimit));
        lines.push_back(horizontalRuler(Columns));

        for (std::size_t row = 0; row < Rows; ++row) {
            ftxui::Elements cells;
            cells.push_back(cellBox(coordinateLabel(row)));
            for (std::size_t column = 0; column < Columns; ++column) {
                const Position position{row, column};
                cells.push_back(cellBox(position == state.agent
                                            ? agentGlyph()
                                            : glyphFor(environment.grid().at(position))));
            }
            lines.push_back(ftxui::hbox(std::move(cells)));
        }

        lines.push_back(footer(recentEvents, environment.endReason()));
        return ftxui::vbox(std::move(lines));
    }

private:
    [[nodiscard]] ftxui::Element statusBar(const Observation& state,
                                          std::size_t totalResources,
                                          std::size_t turnLimit) const;
    [[nodiscard]] ftxui::Element horizontalRuler(std::size_t columns) const;
    [[nodiscard]] ftxui::Element footer(std::span<const NavigationEvent> recentEvents,
                                       EndReason reason) const;

    RenderMode mode_;
};

}  // namespace circuit_escape
