#include <string>
#include <iostream>

#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

// ejecuta: g++ -std=c++23 -Iinclude tests/test1-1.cpp src/environment.cpp src/position.cpp src/controllers.cpp -o tests/test1-1

static void printStep(const std::string& label, const StepResult& result) {
    std::cout << "\n--- " << label << " ---\n";
    std::cout << "Posicion: " << toString(result.observation.agent) << '\n';
    std::cout << "Energia: " << result.observation.energy
              << "/" << result.observation.maximumEnergy << '\n';
    std::cout << "Score: " << result.observation.score << '\n';
    std::cout << "Recursos: " << result.observation.collectedResources << '\n';
    std::cout << "Turno: " << result.observation.turn << '\n';
    std::cout << "Finalizado: " << (result.finished ? "si" : "no") << '\n';

    if (result.finished) {
        std::cout << "Razon: " << toString(result.reason) << '\n';
    }

    std::cout << "Eventos:\n";
    if (result.events.empty()) {
        std::cout << "  (ninguno)\n";
    } else {
        for (const auto& event : result.events) {
            std::cout << "  " << describe(event) << '\n';
        }
    }
}

int main() {
    try {
        // Ajusta esta ruta si tu escenario tiene otro nombre/ubicacion.
        const auto lines = readLines("assets/maps/scenario_01.txt");

        // scenario_1 es de 10x20 por las posiciones
        // . Si el archivo tiene otra dimension, cambia
        // estos dos valores:
        constexpr std::size_t Rows = 10;
        constexpr std::size_t Columns = 20;

        const auto scenario = parseScenario<Rows, Columns>(lines);

        NavigationEnvironment<Rows, Columns> environment(
            scenario.grid,
            scenario.start,
            rulesFor(Difficulty::standard));

        std::cout << "========================================\n";
        std::cout << "       TEST 1-1: ENVIRONMENT\n";
        std::cout << "========================================\n";

        const auto initial = environment.state();

        std::cout << "\n[1] ESTADO INICIAL\n";
        std::cout << "Inicio: " << toString(initial.agent) << '\n';
        std::cout << "Salida: " << toString(initial.goal) << '\n';
        std::cout << "Energia: " << initial.energy
                  << "/" << initial.maximumEnergy << '\n';
        std::cout << "Score: " << initial.score << '\n';
        std::cout << "Recursos recogidos: "
                  << initial.collectedResources << '\n';
        std::cout << "Recursos totales: "
                  << environment.totalResources() << '\n';

        // ------------------------------------------------------------
        // 2. MOVIMIENTO NORMAL
        // ------------------------------------------------------------
        //
        // Usamos la primera accion legal distinta de wait.
        // Esto evita asumir que la celda concreta a la derecha/izquierda
        // es transitable.
        const auto initialActions = initial.availableActions;

        Action firstMove = Action::wait;
        for (const auto action : initialActions) {
            if (action != Action::wait) {
                firstMove = action;
                break;
            }
        }

        if (firstMove != Action::wait) {
            const auto result = environment.step(firstMove);
            printStep("2. MOVIMIENTO VALIDO", result);
        } else {
            std::cout << "\n[2] No hay movimientos validos desde el inicio.\n";
        }

        // ------------------------------------------------------------
        // 3. WAIT
        // ------------------------------------------------------------
        if (!environment.isFinished()) {
            const auto result = environment.step(Action::wait);
            printStep("3. ESPERAR", result);
        }

        // ------------------------------------------------------------
        // 4. MOVIMIENTO INVALIDO
        // ------------------------------------------------------------
        //
        // Buscamos deliberadamente una accion que NO aparezca entre las
        // acciones legales. Si todas son legales, probamos una accion
        // que pueda salir del tablero segun la posicion actual.
        if (!environment.isFinished()) {
            const auto observation = environment.state();

            Action invalid = Action::up;
            const auto isLegal = [&](Action candidate) {
                for (const auto legal : observation.availableActions) {
                    if (legal == candidate) return true;
                }
                return false;
            };

            for (const auto candidate :
                 {Action::up, Action::down, Action::left, Action::right}) {
                if (!isLegal(candidate)) {
                    invalid = candidate;
                    break;
                }
            }

            if (!isLegal(invalid)) {
                const auto result = environment.step(invalid);
                printStep("4. MOVIMIENTO INVALIDO", result);
            } else {
                std::cout << "\n[4] No se encontro un movimiento invalido "
                             "desde esta posicion.\n";
            }
        }

        // ------------------------------------------------------------
        // 5. MOSTRAR ACCIONES LEGALES
        // ------------------------------------------------------------
        if (!environment.isFinished()) {
            const auto observation = environment.state();

            std::cout << "\n[5] ACCIONES LEGALES ACTUALES\n";
            for (const auto action : observation.availableActions) {
                std::cout << "  - " << toString(action) << '\n';
            }
        }

        // ------------------------------------------------------------
        // 6. RESET
        // ------------------------------------------------------------
        std::cout << "\n[6] RESET\n";
        environment.reset(12345);

        const auto afterReset = environment.state();

        std::cout << "Posicion: " << toString(afterReset.agent) << '\n';
        std::cout << "Energia: " << afterReset.energy
                  << "/" << afterReset.maximumEnergy << '\n';
        std::cout << "Score: " << afterReset.score << '\n';
        std::cout << "Recursos recogidos: "
                  << afterReset.collectedResources << '\n';
        std::cout << "Turno: " << afterReset.turn << '\n';
        std::cout << "Semilla: " << environment.seed() << '\n';

        // ------------------------------------------------------------
        // 7. INFORMACION DEL MAPA
        // ------------------------------------------------------------
        std::cout << "\n[7] INFORMACION DEL MAPA\n";
        std::cout << "Recursos totales: "
                  << environment.totalResources() << '\n';
        std::cout << "Recursos pendientes: "
                  << environment.remainingPickups() << '\n';

        std::cout << "\n========================================\n";
        std::cout << "TEST 1-1 TERMINADO\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}