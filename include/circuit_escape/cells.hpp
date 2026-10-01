#pragma once

#include <optional>
#include <type_traits>
#include <variant>

namespace circuit_escape {

// Utilidad variádica para std::visit (hereda de varios callables).
template<typename... Callables>
struct Overloaded : Callables... {
    using Callables::operator()...;
};

template<typename... Callables>
Overloaded(Callables...) -> Overloaded<Callables...>;

// ---------------------------------------------------------------- Celdas
struct Empty {};
struct Wall {};
struct RoughTerrain {};

template<typename Reward>
struct ResourceCell {
    Reward reward{};
    bool collected{false};
};

struct Battery {
    bool consumed{false};
};

struct Trap {};
struct Exit {};

using Cell = std::variant<
    Empty, Wall, RoughTerrain, ResourceCell<int>, Battery, Trap, Exit>;

// ---------------------------------------------------------------- Traits
// Caso general: transitable y no consumible.
template<typename CellType>
struct CellTraits {
    static constexpr bool traversable = true;
    static constexpr bool consumable = false;
};

// Especialización TOTAL: el muro es el único caso no transitable.
template<>
struct CellTraits<Wall> {
    static constexpr bool traversable = false;
    static constexpr bool consumable = false;
};

// Especialización TOTAL: la batería es consumible.
template<>
struct CellTraits<Battery> {
    static constexpr bool traversable = true;
    static constexpr bool consumable = true;
};

// Especialización PARCIAL: toda la familia ResourceCell<Reward> es consumible,
// sin importar cómo se represente la recompensa.
template<typename Reward>
struct CellTraits<ResourceCell<Reward>> {
    static constexpr bool traversable = true;
    static constexpr bool consumable = true;
};

[[nodiscard]] inline bool isTraversable(const Cell& cell) {
    return std::visit(
        [](const auto& value) {
            return CellTraits<std::decay_t<decltype(value)>>::traversable;
        },
        cell);
}

[[nodiscard]] inline bool isConsumable(const Cell& cell) {
    return std::visit(
        [](const auto& value) {
            return CellTraits<std::decay_t<decltype(value)>>::consumable;
        },
        cell);
}

// Una celda consumible cuyo efecto ya fue usado.
[[nodiscard]] inline bool isSpent(const Cell& cell) {
    return std::visit(
        Overloaded{
            [](const ResourceCell<int>& r) { return r.collected; },
            [](const Battery& b) { return b.consumed; },
            [](const auto&) { return false; }},
        cell);
}

// Consumible que todavía no fue activado.
[[nodiscard]] inline bool isPendingPickup(const Cell& cell) {
    return isConsumable(cell) && !isSpent(cell);
}

// ---------------------------------------------- Formato textual de mapas
//   '#' muro   '.' libre   '~' terreno elevado   'R' recurso
//   'B' batería   'T' trampa   'S' salida   '@' inicio (celda libre)
[[nodiscard]] inline std::optional<Cell> cellFromChar(char symbol) {
    switch (symbol) {
        case '.':
        case '@': return Cell{Empty{}};
        case '#': return Cell{Wall{}};
        case '~': return Cell{RoughTerrain{}};
        case 'R': return Cell{ResourceCell<int>{}};
        case 'B': return Cell{Battery{}};
        case 'T': return Cell{Trap{}};
        case 'S': return Cell{Exit{}};
        default: return std::nullopt;
    }
}

}  // namespace circuit_escape