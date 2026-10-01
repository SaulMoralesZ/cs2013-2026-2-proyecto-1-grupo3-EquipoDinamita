#include <iostream>

#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

char cellSymbol(const Cell& cell) {
    return std::visit(
        Overloaded{
            [](const Empty&) { return '.'; },
            [](const Wall&) { return '#'; },
            [](const RoughTerrain&) { return '~'; },
            [](const ResourceCell<int>& r) {
                return r.collected ? '.' : 'R';
            },
            [](const Battery& b) {
                return b.consumed ? '.' : 'B';
            },
            [](const Trap&) { return 'T'; },
            [](const Exit&) { return 'S'; }
        },
        cell
    );
}

int main() {

    try {
        constexpr std::size_t Rows = 10;
        constexpr std::size_t Columns = 20;

        const auto lines =
            readLines("assets/maps/scenario_01.txt");

        const auto scenario =
            parseScenario<Rows, Columns>(lines);

        NavigationEnvironment<Rows, Columns> game(
            scenario.grid,
            scenario.start
        );

        std::cout << "Escenario cargado correctamente.\n";
        std::cout << "Inicio: "
                  << toString(game.start()) << '\n';

        std::cout << "Salida: "
                  << toString(game.goal()) << '\n';

        std::cout << "Energia: "
                  << game.state().energy
                  << '/'
                  << game.state().maximumEnergy
                  << '\n';

        std::cout << "Recursos: "
                  << game.totalResources()
                  << '\n';

    } catch (const std::exception& e) {

        std::cerr << "ERROR: "
                  << e.what()
                  << '\n';

        return 1;
    }

    return 0;
}

// ejecutar: g++ -std=c++23 -Iinclude tests/test1.cpp src/environment.cpp src/position.cpp src/controllers.cpp -o tests/test1