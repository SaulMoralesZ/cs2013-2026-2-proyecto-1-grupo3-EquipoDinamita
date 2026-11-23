#undef NDEBUG
#include <cassert>
#include <list>
#include <utility>
#include <stdexcept>
#include <variant>
#include <vector>

#include "../include/circuit_escape/cells.hpp"
#include "../include/circuit_escape/grid.hpp"

using namespace circuit_escape;

namespace {

void accesoValidoEInvalido() {
    Grid<Cell> grid(3,4);
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
    const Grid<Cell> grid(3,4);
    assert(grid.contains({0, 0}));
    assert(grid.contains({0, 3}));
    assert(grid.contains({2, 0}));
    assert(grid.contains({2, 3}));
    assert(!grid.contains({3, 3}));
    assert(!grid.contains({2, 4}));

    assert(grid.rows() == 3);
    assert(grid.columns() == 4);
}

void recorridoPorFilas() {
    Grid<Cell> grid(3,4);
    grid.at({1, 2}) = Exit{};

    assert((grid.positionOf(6) == Position{1, 2}));

    const auto salida = findPosition( grid, [](const Cell& cell) {
        return std::holds_alternative<Exit>(cell);
    });
    assert(salida.has_value());
    assert((*salida == Position{1, 2}));

    const auto inexistente = findPosition(
        grid, [](const Cell& cell) { return std::holds_alternative<Trap>(cell); });
    assert(!inexistente.has_value());
}

void dimensionesValidas() {
    for (const auto& [filas, columnas] : {std::pair<std::size_t, std::size_t>{3, 4},
                                          {10, 15}, {20, 30}, {40, 60}, {1, 1}}) {
        const Grid<Cell> grid(filas, columnas);
        assert(grid.rows() == filas);
        assert(grid.columns() == columnas);
        assert(countCells(grid.begin(), grid.end(),[](const Cell& cell) {
            return std::holds_alternative<Empty>(cell); }) == filas*columnas);
    }
}

void dimensionesInvalidas() {
    const auto rechaza = [](std::size_t filas, std::size_t columnas) {
        try {
            const Grid<Cell> grid(filas, columnas);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    assert(rechaza(0, 5));
    assert(rechaza(5, 0));
    assert(rechaza(0, 0));
    assert(rechaza(Grid<Cell>::max_side + 1, 5));
    assert(rechaza(5, Grid<Cell>::max_side + 1));
    assert(rechaza(static_cast<std::size_t>(-1), 5));
    assert(!rechaza(Grid<Cell>::max_side, Grid<Cell>::max_side));
}

void vecindad() {
    const Grid<Cell> grid(3,4);
    const auto centro = grid.neighbor({1,1}, Action::up);

    assert(centro && *centro == (Position{0, 1}));
    assert(!grid.neighbor({0, 0}, Action::up));
    assert(!grid.neighbor({0, 0}, Action::left));
    assert(!grid.neighbor({2, 3}, Action::down));
    assert(!grid.neighbor({2, 3}, Action::right));
    assert(grid.neighbor({2, 3}, Action::up).has_value());
    assert(grid.neighbor({2, 3}, Action::left).has_value());

    const Grid<Cell> unica(1, 1);
    for (Action action : {Action::up, Action::down, Action::left, Action::right}) {
        assert(!unica.neighbor({0, 0}, action));
    }
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
    dimensionesValidas();
    dimensionesInvalidas();
    vecindad();
    recorridoPorFilas();
    rangoVacio();
    dosContenedoresDistintos();
    return 0;
}