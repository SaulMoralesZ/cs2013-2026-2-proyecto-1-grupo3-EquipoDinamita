#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <ftxui/screen/screen.hpp>

#include "circuit_escape/console_ui.hpp"

using namespace circuit_escape;

namespace {

bool traduceA(const ConsoleUI& ui, const ftxui::Event& event, Action esperada) {
    const auto comando = ui.translate(event);
    if (!comando) return false;
    const auto* accion = std::get_if<Action>(&*comando);
    return accion != nullptr && *accion == esperada;
}

void teclasDeMovimientoSinDistinguirMayusculas() {
    const ConsoleUI ui;
    for (const char* letra : {"w", "W"}) assert(traduceA(ui, ftxui::Event::Character(letra), Action::up));
    for (const char* letra : {"s", "S"}) assert(traduceA(ui, ftxui::Event::Character(letra), Action::down));
    for (const char* letra : {"a", "A"}) assert(traduceA(ui, ftxui::Event::Character(letra), Action::left));
    for (const char* letra : {"d", "D"}) assert(traduceA(ui, ftxui::Event::Character(letra), Action::right));
    for (const char* letra : {"e", "E"}) assert(traduceA(ui, ftxui::Event::Character(letra), Action::wait));
}

void flechas() {
    const ConsoleUI ui;
    assert(traduceA(ui, ftxui::Event::ArrowUp, Action::up));
    assert(traduceA(ui, ftxui::Event::ArrowDown, Action::down));
    assert(traduceA(ui, ftxui::Event::ArrowLeft, Action::left));
    assert(traduceA(ui, ftxui::Event::ArrowRight, Action::right));
}

void ayudaYSalida() {
    const ConsoleUI ui;
    for (const char* letra : {"h", "H"}) {
        const auto comando = ui.translate(ftxui::Event::Character(letra));
        assert(comando && std::holds_alternative<HelpCommand>(*comando));
    }
    for (const char* letra : {"q", "Q"}) {
        const auto comando = ui.translate(ftxui::Event::Character(letra));
        assert(comando && std::holds_alternative<QuitCommand>(*comando));
    }
}

void comandoDesconocidoSeRechaza() {
    const ConsoleUI ui;
    assert(!ui.translate(ftxui::Event::Character("x")));
    assert(!ui.translate(ftxui::Event::Character("1")));
    assert(!ui.translate(ftxui::Event::Character("wa")));
    assert(!ui.translate(ftxui::Event::Tab));
}

void soloElTecladoCuentaComoComando() {
    assert(ConsoleUI::isKeyboardEvent(ftxui::Event::Character("w")));
    assert(ConsoleUI::isKeyboardEvent(ftxui::Event::Character("x")));
    assert(ConsoleUI::isKeyboardEvent(ftxui::Event::ArrowUp));
    assert(ConsoleUI::isKeyboardEvent(ftxui::Event::Escape));
    assert(ConsoleUI::isKeyboardEvent(ftxui::Event::Tab));

    ftxui::Mouse movimiento;
    movimiento.button = ftxui::Mouse::None;
    movimiento.motion = ftxui::Mouse::Moved;
    assert(!ConsoleUI::isKeyboardEvent(ftxui::Event::Mouse("", movimiento)));

    ftxui::Mouse rueda;
    rueda.button = ftxui::Mouse::WheelUp;
    rueda.motion = ftxui::Mouse::Pressed;
    assert(!ConsoleUI::isKeyboardEvent(ftxui::Event::Mouse("", rueda)));

    assert(!ConsoleUI::isKeyboardEvent(ftxui::Event::Custom));
}

std::vector<Cell> lasSieteCeldas() {
    return {Cell{Empty{}},          Cell{Wall{}},    Cell{RoughTerrain{}},
            Cell{ResourceCell<int>{}}, Cell{Battery{}}, Cell{Trap{}},
            Cell{Exit{}}};
}

void glifosDistintosEnAmbosModos() {
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        const ConsoleUI ui(modo);
        std::set<std::string> vistos;
        for (const Cell& celda : lasSieteCeldas()) {
            const std::string glifo = ui.glyphFor(celda);
            assert(!glifo.empty());
            vistos.insert(glifo);
        }
        assert(vistos.size() == 7);
        assert(vistos.count(ui.agentGlyph()) == 0);
    }
}

void glifosAsciiExactos() {
    const ConsoleUI ui(RenderMode::ascii);
    assert(ui.glyphFor(Cell{Empty{}}) == ".");
    assert(ui.glyphFor(Cell{Wall{}}) == "#");
    assert(ui.glyphFor(Cell{RoughTerrain{}}) == "~");
    assert(ui.glyphFor(Cell{ResourceCell<int>{}}) == "R");
    assert(ui.glyphFor(Cell{Battery{}}) == "B");
    assert(ui.glyphFor(Cell{Trap{}}) == "T");
    assert(ui.glyphFor(Cell{Exit{}}) == "S");
    assert(ui.agentGlyph() == "@");
}

void consumiblesUsadosSeVenLibres() {
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        const ConsoleUI ui(modo);
        assert(ui.glyphFor(Cell{ResourceCell<int>{10, true}}) == ui.emptyGlyph());
        assert(ui.glyphFor(Cell{Battery{true}}) == ui.emptyGlyph());
    }
}

void tableroDe20x30Ocupa62Columnas() {
    std::vector<std::string> lineas(20, std::string(30, '.'));
    lineas[0][0] = '@';
    lineas[19][29] = 'S';
    const auto escenario = parseScenario<20, 30>(lineas);
    const NavigationEnvironment<20, 30> entorno(escenario.grid, escenario.start);
    const std::vector<NavigationEvent> sinEventos;

    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        const ConsoleUI ui(modo);
        auto documento = ui.render(entorno, sinEventos);
        auto pantalla = ftxui::Screen::Create(ftxui::Dimension::Fit(documento));
        ftxui::Render(pantalla, documento);
        assert(pantalla.dimx() == 62);
        assert(pantalla.dimy() == 23);
    }
}

}

int main() {
    teclasDeMovimientoSinDistinguirMayusculas();
    flechas();
    ayudaYSalida();
    comandoDesconocidoSeRechaza();
    glifosDistintosEnAmbosModos();
    glifosAsciiExactos();
    consumiblesUsadosSeVenLibres();
    tableroDe20x30Ocupa62Columnas();
    soloElTecladoCuentaComoComando();
    return 0;
}