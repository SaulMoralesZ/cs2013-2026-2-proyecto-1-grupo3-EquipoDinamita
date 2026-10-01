#ifndef CIRCUIT_ESCAPE_AGENT_HPP
#define CIRCUIT_ESCAPE_AGENT_HPP

#include <cstddef>
#include <stdexcept>

#include "./position.hpp"

namespace circuit_escape {

// Estado del agente. Invariantes que preserva:
//   * 0 <= energía <= energía máxima (toda modificación se acota);
//   * un agente finalizado no puede moverse ni modificarse;
//   * la pertenencia al tablero y la celda transitable las garantiza el entorno.
class Agent {
public:
    Agent(Position start, int energy, int maximumEnergy)
        : position_(start), energy_(energy), maximumEnergy_(maximumEnergy) {
        if (maximumEnergy <= 0 || energy < 0 || energy > maximumEnergy) {
            throw std::invalid_argument("Energia inicial o maxima invalida");
        }
    }

    [[nodiscard]] Position position() const noexcept { return position_; }
    [[nodiscard]] int energy() const noexcept { return energy_; }
    [[nodiscard]] int maximumEnergy() const noexcept { return maximumEnergy_; }
    [[nodiscard]] int score() const noexcept { return score_; }
    [[nodiscard]] std::size_t collectedResources() const noexcept { return collected_; }
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    void moveTo(Position destination) {
        requireActive();
        position_ = destination;
    }

    // Suma `delta` (positivo o negativo) acotando al intervalo [0, máximo].
    // Retorna la energía resultante.
    int adjustEnergy(int delta) {
        requireActive();
        const int raw = energy_ + delta;
        energy_ = raw < 0 ? 0 : (raw > maximumEnergy_ ? maximumEnergy_ : raw);
        return energy_;
    }

    void addScore(int delta) {
        requireActive();
        score_ += delta;
    }

    void collectResource() {
        requireActive();
        ++collected_;
    }

    void finish() noexcept { active_ = false; }

private:
    void requireActive() const {
        if (!active_) throw std::logic_error("El agente ya finalizo");
    }

    Position position_;
    int energy_;
    int maximumEnergy_;
    int score_{0};
    std::size_t collected_{0};
    bool active_{true};
};

}  // namespace circuit_escape

#endif  // CIRCUIT_ESCAPE_AGENT_HPP