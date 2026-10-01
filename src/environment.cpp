#include "../include/circuit_escape/environment.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace circuit_escape {

std::vector<std::string> readLines(
    const std::filesystem::path& path) {

    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error(
            "No se pudo abrir el escenario: " +
            path.string());
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

std::string toString(EndReason reason) {
    switch (reason) {
        case EndReason::none:
            return "en curso";

        case EndReason::goalReached:
            return "objetivo alcanzado";

        case EndReason::noEnergy:
            return "sin energia";

        case EndReason::turnLimit:
            return "limite de turnos";
    }

    return "desconocido";
}

std::string describe(const NavigationEvent& event) {
    return std::visit(
        [](const auto& e) -> std::string {

            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, MovedEvent>) {
                return "Movimiento: " +
                       toString(e.from) +
                       " -> " +
                       toString(e.to);
            }

            else if constexpr (
                std::is_same_v<T, MovementRejectedEvent>) {
                return "Movimiento rechazado";
            }

            else if constexpr (
                std::is_same_v<T, ResourceCollectedEvent>) {
                return "Recurso recogido: +" +
                       std::to_string(e.points) +
                       " puntos";
            }

            else if constexpr (
                std::is_same_v<T, EnergyChangedEvent>) {
                return "Energia: " +
                       std::to_string(e.previous) +
                       " -> " +
                       std::to_string(e.current);
            }

            else if constexpr (
                std::is_same_v<T, TrapTriggeredEvent>) {
                return "Trampa activada";
            }

            else if constexpr (
                std::is_same_v<T, GoalReachedEvent>) {
                return "Salida alcanzada";
            }

            return "Evento desconocido";
        },
        event);
}

}