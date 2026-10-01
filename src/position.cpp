#include "../include/circuit_escape/position.hpp"

#include <sstream>

namespace circuit_escape {

std::optional<Position> neighbor(Position origin, Action action) {
    switch (action) {
        case Action::up:
            if (origin.row == 0)
                return std::nullopt;
            return Position{origin.row - 1, origin.column};

        case Action::down:
            return Position{origin.row + 1, origin.column};

        case Action::left:
            if (origin.column == 0)
                return std::nullopt;
            return Position{origin.row, origin.column - 1};

        case Action::right:
            return Position{origin.row, origin.column + 1};

        case Action::wait:
            return origin;
    }

    return std::nullopt;
}

std::optional<Action> actionBetween(Position from, Position to) {
    if (to.row + 1 == from.row &&
        to.column == from.column)
        return Action::up;

    if (to.row == from.row + 1 &&
        to.column == from.column)
        return Action::down;

    if (to.column + 1 == from.column &&
        to.row == from.row)
        return Action::left;

    if (to.column == from.column + 1 &&
        to.row == from.row)
        return Action::right;

    return std::nullopt;
}

std::size_t manhattanDistance(Position a, Position b) noexcept {
    const auto rowDistance =
        a.row > b.row ? a.row - b.row : b.row - a.row;

    const auto columnDistance =
        a.column > b.column ? a.column - b.column : b.column - a.column;

    return rowDistance + columnDistance;
}

std::string toString(Position position) {
    std::ostringstream out;
    out << '(' << position.row << ", " << position.column << ')';
    return out.str();
}

std::string toString(Action action) {
    switch (action) {
        case Action::up:
            return "up";
        case Action::down:
            return "down";
        case Action::left:
            return "left";
        case Action::right:
            return "right";
        case Action::wait:
            return "wait";
    }

    return "unknown";
}

} // namespace circuit_escape