#undef NDEBUG
#include <cassert>
#include <list>
#include <stdexcept>
#include <variant>
#include <vector>

#include "../include/circuit_escape/environment.hpp"

using namespace circuit_escape;

namespace {

void accesoValidoEInvalido() {
    Grid<Cell, 3, 4> grid;
    grid.at({0, 0}) = Wall{};
    assert(std::holds_alternative<Wall>(grid.at({0, 0})));
    assert(std::holds_alternative<Empty>(grid.at({2, 3})));

    bool lanzoPorFila = false;
    try {
        (void)grid.at({3, 0});
    } catch (const std::out_of_range&) {
        lanzoPorFila = true;
    }
    assert(lanzoPorFila);

    bool lanzoPorColumna = false;
    try {
        (void)grid.at({0, 4});
    } catch (const std::out_of_range&) {
        lanzoPorColumna = true;
    }
    assert(lanzoPorColumna);
}

void bordesYEsquinas() {
    const Grid<Cell, 3, 4> grid;
    assert(grid.contains({0, 0}));
    assert(grid.contains({0, 3}));
    assert(grid.contains({2, 0}));
    assert(grid.contains({2, 3}));
    assert(!grid.contains({3, 3}));
    assert(!grid.contains({2, 4}));

    static_assert(Grid<Cell, 3, 4>::rows() == 3);
    static_assert(Grid<Cell, 3, 4>::columns() == 4);
}

void recorridoPorFilas() {
    Grid<Cell, 3, 4> grid;
    grid.at({1, 2}) = Exit{};

    assert((Grid<Cell, 3, 4>::positionOf(6) == Position{1, 2}));

    const auto salida = findPosition(
        grid, [](const Cell& cell) { return std::holds_alternative<Exit>(cell); });
    assert(salida.has_value());
    assert((*salida == Position{1, 2}));

    const auto inexistente = findPosition(
        grid, [](const Cell& cell) { return std::holds_alternative<Trap>(cell); });
    assert(!inexistente.has_value());
}

void rangoVacio() {
    const std::vector<Cell> sinCeldas;
    assert(countCells(sinCeldas.begin(), sinCeldas.end(),
                      [](const Cell&) { return true; }) == 0);
    assert(findFirstCell(sinCeldas.begin(), sinCeldas.end(),
                         [](const Cell&) { return true; }) == sinCeldas.end());
}

void dosContenedoresDistintos() {
    const std::vector<Position> comoVector{{0, 0}, {1, 1}, {0, 2}};
    const std::list<Position> comoLista{{0, 0}, {1, 1}, {0, 2}};
    const auto enPrimeraFila = [](Position position) { return position.row == 0; };

    assert(countCells(comoVector.begin(), comoVector.end(), enPrimeraFila) == 2);
    assert(countCells(comoLista.begin(), comoLista.end(), enPrimeraFila) == 2);
}

}

int main() {
    accesoValidoEInvalido();
    bordesYEsquinas();
    recorridoPorFilas();
    rangoVacio();
    dosContenedoresDistintos();
    return 0;
}
