#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace circuit_escape {

enum class Difficulty { easy, standard, hard };

// Todos los valores numéricos del juego viven aquí (son datos, no ramas).
struct GameRules {
    int initialEnergy{60};
    int maximumEnergy{60};
    std::size_t turnLimit{180};

    int baseCost{1};     // libre, recurso, batería, trampa, salida
    int roughCost{2};    // terreno elevado
    int waitCost{1};
    int invalidCost{1};  // intento hacia muro o fuera del tablero

    int resourcePoints{10};
    int batteryRecharge{3};
    int trapEnergyPenalty{2};
    int trapScorePenalty{1};

    friend bool operator==(const GameRules&, const GameRules&) = default;
};

[[nodiscard]] constexpr GameRules rulesFor(Difficulty difficulty) noexcept {
    switch (difficulty) {
        case Difficulty::easy:
            return GameRules{80, 80, 240, 1, 2, 1, 1, 15, 5, 1, 0};
        case Difficulty::hard:
            return GameRules{40, 40, 140, 1, 3, 1, 1, 8, 2, 3, 2};
        case Difficulty::standard:
            break;
    }
    return GameRules{60, 60, 180, 1, 2, 1, 1, 10, 3, 2, 1};
}

[[nodiscard]] constexpr std::string_view toString(Difficulty difficulty) noexcept {
    switch (difficulty) {
        case Difficulty::easy: return "easy";
        case Difficulty::standard: return "standard";
        case Difficulty::hard: return "hard";
    }
    return "standard";
}

[[nodiscard]] constexpr std::optional<Difficulty> parseDifficulty(std::string_view name) noexcept {
    if (name == "easy") return Difficulty::easy;
    if (name == "standard") return Difficulty::standard;
    if (name == "hard") return Difficulty::hard;
    return std::nullopt;
}

}  // namespace circuit_escape