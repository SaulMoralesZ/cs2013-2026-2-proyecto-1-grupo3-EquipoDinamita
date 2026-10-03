#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <ftxui/screen/screen.hpp>

#include "../include/circuit_escape/console_ui.hpp"

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
    movimiento.motion = ftxui::Mouse::Pressed;
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

void escenariosRealesSeDibujanEn20x30() {
    for (const char* ruta : {"assets/maps/scenario_01.txt", "assets/maps/scenario_02.txt"}) {
        const auto escenario = parseScenario<20, 30>(readLines(ruta));
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

void pieResumeCadaTipoDeEvento() {
    const ConsoleUI ui(RenderMode::ascii);
    const auto texto = [&](std::vector<NavigationEvent> eventos) {
        return ui.lastEventText(eventos);
    };

    assert(texto({}) == "sin eventos");
    // movimiento normal
    assert(texto({MovedEvent{{1, 1}, {1, 2}, 1}, EnergyChangedEvent{60, 59}}) == "@ en (1,2)");
    // recurso
    assert(texto({MovedEvent{{10, 23}, {10, 24}, 1}, EnergyChangedEvent{60, 59},
                  ResourceCollectedEvent{{10, 24}, 10}}) == "R +10 en (10,24)");
    // batería: primero se paga la entrada, luego se recarga
    assert(texto({MovedEvent{{1, 1}, {1, 2}, 1}, EnergyChangedEvent{50, 49},
                  EnergyChangedEvent{49, 52}}) == "B +3 energia");
    // trampa
    assert(texto({MovedEvent{{4, 3}, {4, 4}, 1}, EnergyChangedEvent{60, 59},
                  TrapTriggeredEvent{{4, 4}}, EnergyChangedEvent{59, 57}}) == "T trampa en (4,4)");
    // intento inválido
    assert(texto({MovementRejectedEvent{{1, 1}, Action::up}, EnergyChangedEvent{60, 59}}) ==
           "movimiento bloqueado hacia arriba");
    // esperar
    assert(texto({EnergyChangedEvent{60, 59}}) == "energia 60 -> 59");
    // salida
    assert(texto({MovedEvent{{18, 27}, {18, 28}, 1}, EnergyChangedEvent{5, 4},
                  GoalReachedEvent{{18, 28}}}) == "S salida alcanzada en (18,28)");
}

void pieEnModoEmojiUsaElGlifoDeLaCelda() {
    const ConsoleUI ui(RenderMode::emoji);
    const std::vector<NavigationEvent> recurso{ResourceCollectedEvent{{10, 24}, 10}};
    assert(ui.lastEventText(recurso) == ui.glyphFor(Cell{ResourceCell<int>{}}) + " +10 en (10,24)");
}

void pieConEventosRealesDelEntorno() {
    std::vector<std::string> lineas(20, std::string(30, '.'));
    lineas[0][0] = '@';
    lineas[0][1] = 'R';
    lineas[19][29] = 'S';
    const auto escenario = parseScenario<20, 30>(lineas);
    NavigationEnvironment<20, 30> entorno(escenario.grid, escenario.start);

    const auto resultado = entorno.step(Action::right);
    assert(ConsoleUI(RenderMode::ascii).lastEventText(resultado.events) == "R +10 en (0,1)");
}

}

int main() {
    teclasDeMovimientoSinDistinguirMayusculas();
    flechas();
    ayudaYSalida();
    comandoDesconocidoSeRechaza();
    pieResumeCadaTipoDeEvento();
    pieEnModoEmojiUsaElGlifoDeLaCelda();
    pieConEventosRealesDelEntorno();
    glifosDistintosEnAmbosModos();
    glifosAsciiExactos();
    consumiblesUsadosSeVenLibres();
    tableroDe20x30Ocupa62Columnas();
    soloElTecladoCuentaComoComando();
    escenariosRealesSeDibujanEn20x30();
    return 0;
}