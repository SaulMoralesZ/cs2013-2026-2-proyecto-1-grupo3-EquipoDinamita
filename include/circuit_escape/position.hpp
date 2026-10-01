#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace circuit_escape {

struct Position {
    std::size_t row{};
    std::size_t column{};

    friend bool operator==(const Position&, const Position&) = default;
};

struct PositionHash {
    std::size_t operator()(const Position& p) const noexcept {
        const std::size_t h1 = std::hash<std::size_t>{}(p.row);
        const std::size_t h2 = std::hash<std::size_t>{}(p.column);

        return h1 ^
               (h2 + 0x9e3779b97f4a7c15ULL +
                (h1 << 6) + (h1 >> 2));
    }
};

enum class Action {
    up,
    down,
    left,
    right,
    wait
};

std::optional<Position> neighbor(Position origin, Action action);

std::optional<Action> actionBetween(Position from, Position to);

std::size_t manhattanDistance(Position a, Position b) noexcept;

std::string toString(Position position);

std::string toString(Action action);

} // namespace circuit_escape