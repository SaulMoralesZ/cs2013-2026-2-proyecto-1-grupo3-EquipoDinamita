#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "./environment.hpp"
#include "./grid.hpp"

namespace circuit_escape {

// ------------------------------------------------- Interfaz polimórfica
class IController {
public:
    virtual ~IController() = default;
    virtual Action selectAction(const Observation& observation,
                                std::span<const Action> legalActions) = 0;
};

// Contrato en compilación: una política elige una Action a partir de la
// observación y las acciones legales.
template<typename Policy>
concept NavigationPolicy = requires(Policy& policy,
                                    const Observation& observation,
                                    std::span<const Action> actions) {
    { policy.selectAction(observation, actions) } -> std::same_as<Action>;
};

// Adaptador genérico: cualquier NavigationPolicy pasa a ser un IController.
// El despacho dinámico ocurre en la llamada virtual selectAction.
template<NavigationPolicy Policy>
class PolicyController final : public IController {
public:
    explicit PolicyController(Policy policy) : policy_(std::move(policy)) {}

    Action selectAction(const Observation& observation,
                        std::span<const Action> legalActions) override {
        return policy_.selectAction(observation, legalActions);
    }

private:
    Policy policy_;
};

// Mejor acción de [first, last) según score (mayor es mejor; en empate gana
// la primera). Funciona con vector, array, span, list, etc.
template<typename InputIt, typename ScoreFn>
[[nodiscard]] std::optional<Action> bestAction(InputIt first, InputIt last, ScoreFn score) {
    if (first == last) return std::nullopt;
    Action best = *first;
    auto bestScore = score(best);
    for (++first; first != last; ++first) {
        auto candidate = score(*first);
        if (candidate > bestScore) {
            bestScore = candidate;
            best = *first;
        }
    }
    return best;
}

// --------------------------------------------------------- Políticas
// Aleatoria con semilla controlable. Evita wait si existe otro movimiento.
class RandomPolicy {
public:
    explicit RandomPolicy(std::uint32_t seed) : rng_(seed) {}
    Action selectAction(const Observation& observation,
                        std::span<const Action> legalActions);

private:
    std::mt19937 rng_;
};

// Heurística: reduce la distancia Manhattan a la salida y penaliza las
// celdas ya visitadas para no quedar atrapada en un callejón.
class HeuristicPolicy {
public:
    Action selectAction(const Observation& observation,
                        std::span<const Action> legalActions);

private:
    std::unordered_map<Position, int, PositionHash> visits_;
};

static_assert(NavigationPolicy<RandomPolicy>);
static_assert(NavigationPolicy<HeuristicPolicy>);

// ------------------------------------------------------ Controlador humano
// La interfaz entrega la decisión con submit(); no lee std::cin.
class HumanController final : public IController {
public:
    void submit(Action action) noexcept { pending_ = action; }
    Action selectAction(const Observation& observation,
                        std::span<const Action> legalActions) override;

private:
    std::optional<Action> pending_;
};

// ---------------------------------------------------------------- Fábrica
enum class ControllerKind { random, heuristic };

[[nodiscard]] std::optional<ControllerKind> parseControllerKind(std::string_view name);
[[nodiscard]] std::string_view toString(ControllerKind kind) noexcept;
[[nodiscard]] std::unique_ptr<IController> makeController(ControllerKind kind,
                                                          std::uint32_t seed);

}  // namespace circuit_escape