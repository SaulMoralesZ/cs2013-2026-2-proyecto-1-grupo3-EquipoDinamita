#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace circuit_escape {

// Fila y columna de una celda. No conoce el tamaño del tablero.
struct Position {
    std::size_t row{};
    std::size_t column{};

    friend bool operator==(const Position&, const Position&) = default;
    friend auto operator<=>(const Position&, const Position&) = default;
};

enum class Action { up, down, left, right, wait };

// Posición vecina tras aplicar la acción. Retorna std::nullopt si el cálculo
// produciría una fila o columna negativa. El límite superior lo valida Grid.
// Action::wait retorna la misma posición.
[[nodiscard]] std::optional<Position> neighbor(Position origin, Action action);

// Acción que lleva de `from` a `to` si son vecinas directas (distancia 1).
[[nodiscard]] std::optional<Action> actionBetween(Position from, Position to);

// Distancia Manhattan entre dos posiciones.
[[nodiscard]] std::size_t manhattanDistance(Position a, Position b) noexcept;

[[nodiscard]] std::string toString(Position position);
[[nodiscard]] std::string toString(Action action);

// Permite usar Position como clave de std::unordered_map / unordered_set.
struct PositionHash {
    [[nodiscard]] std::size_t operator()(const Position& p) const noexcept {
        const std::size_t h1 = std::hash<std::size_t>{}(p.row);
        const std::size_t h2 = std::hash<std::size_t>{}(p.column);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

}  // namespace circuit_escape