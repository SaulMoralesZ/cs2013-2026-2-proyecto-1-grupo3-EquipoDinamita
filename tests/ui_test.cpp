#undef NDEBUG
#include <cassert>
#include <cstddef>
#include <set>
#include <utility>
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
        const auto escenario = parseScenario(readLines(ruta));
        const NavigationEnvironment entorno(escenario.grid, escenario.start);
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
    const auto escenario = parseScenario(lineas);
    const NavigationEnvironment entorno(escenario.grid, escenario.start);
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

void tableroDeCualquierTamanoConservaFilasYColumnas() {
    for (const auto& [filas, columnas] : {std::pair<std::size_t, std::size_t>{10, 15},
                                          {20, 30}, {40, 60}, {3, 4}}) {
        std::vector<std::string> lineas(filas, std::string(columnas, '.'));
        lineas[0][0] = '@';
        lineas[filas - 1][columnas - 1] = 'S';
        const auto escenario = parseScenario(lineas);
        const NavigationEnvironment entorno(escenario.grid, escenario.start);
        const std::vector<NavigationEvent> sinEventos;

        for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
            const ConsoleUI ui(modo);
            // Tamaño natural del elemento: no depende del tamaño de la terminal
            // (Dimension::Fit recorta a la terminal y 40x60 no cabe en 80x24).
            auto documento = ui.render(entorno, sinEventos);
            documento->ComputeRequirement();
            assert(documento->requirement().min_x >= static_cast<int>(2 * (columnas + 1)));
            assert(documento->requirement().min_y == static_cast<int>(filas + 3));
        }
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
    const auto escenario = parseScenario(lineas);
    NavigationEnvironment entorno(escenario.grid, escenario.start);

    const auto resultado = entorno.step(Action::right);
    assert(ConsoleUI(RenderMode::ascii).lastEventText(resultado.events) == "R +10 en (0,1)");
}

// ---------------------------------------------------- Ayuda H y comandos
void laAyudaNoAvanzaTurnos() {
    const UiCommand mover{Action::up};
    const UiCommand esperar{Action::wait};
    const UiCommand ayuda{HelpCommand{}};
    const UiCommand salir{QuitCommand{}};

    // Ayuda cerrada
    assert(resolve_command(mover, false, false) == UiOutcome::play_action);
    assert(resolve_command(esperar, false, false) == UiOutcome::play_action);
    assert(resolve_command(ayuda, false, false) == UiOutcome::open_help);
    assert(resolve_command(salir, false, false) == UiOutcome::quit);   // Q
    assert(resolve_command(salir, false, true) == UiOutcome::quit);    // Esc sin ayuda

    // Ayuda abierta: nada de esto juega un turno
    assert(resolve_command(mover, true, false) == UiOutcome::close_help);
    assert(resolve_command(esperar, true, false) == UiOutcome::close_help);
    assert(resolve_command(ayuda, true, false) == UiOutcome::close_help);   // H cierra
    assert(resolve_command(salir, true, true) == UiOutcome::close_help);    // Esc cierra
    assert(resolve_command(salir, true, false) == UiOutcome::quit);         // Q sigue saliendo
}

void escapeSeTraduceAComandoDeSalida() {
    const ConsoleUI ui;
    const auto comando = ui.translate(ftxui::Event::Escape);
    assert(comando && std::holds_alternative<QuitCommand>(*comando));
}

// ------------------------------------------- Vigilante e interruptores en pantalla
Scenario escenarioConVigilante() {
    Grid<Cell> grid(4, 8);
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 8; ++c) {
            const bool borde = r == 0 || r == 3 || c == 0 || c == 7;
            grid.at({r, c}) = borde ? Cell{Wall{}} : Cell{Empty{}};
        }
    }
    grid.at({1, 5}) = Exit{};
    Scenario escenario{std::move(grid)};
    escenario.start = {1, 1};
    escenario.switch_a = {1, 2};
    escenario.switch_b = {1, 3};
    escenario.guard_start = {2, 6};
    return escenario;
}

// Dibuja en una pantalla fija (independiente de la terminal) y devuelve el contenido.
ftxui::Screen dibujar(const ConsoleUI& ui, const NavigationEnvironment& entorno) {
    const std::vector<NavigationEvent> sinEventos;
    auto documento = ui.render(entorno, sinEventos);
    auto pantalla = ftxui::Screen::Create(ftxui::Dimension::Fixed(130), ftxui::Dimension::Fixed(12));
    ftxui::Render(pantalla, documento);
    return pantalla;
}

// En modo ASCII cada celda ocupa 2 columnas; la fila 0 y la 1 son barra de estado y
// linea de interruptores, y la regla horizontal va debajo.
std::string glifoEn(const ftxui::Screen& pantalla, std::size_t fila, std::size_t columna,
                    std::size_t lineasSuperiores) {
    return pantalla.at(static_cast<int>(2 * (columna + 1)),
                       static_cast<int>(fila + lineasSuperiores + 1));
}

void glifosDelVigilanteYLosInterruptores() {
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        const ConsoleUI ui(modo);
        std::set<std::string> vistos;
        for (const Cell& celda : lasSieteCeldas()) vistos.insert(ui.glyphFor(celda));
        vistos.insert(ui.agentGlyph());
        vistos.insert(ui.guardGlyph());
        vistos.insert(ui.switchGlyph(false));
        vistos.insert(ui.switchGlyph(true));
        assert(vistos.size() == 11);   // 7 celdas + agente + vigilante + 2 estados
    }
    const ConsoleUI ascii(RenderMode::ascii);
    assert(ascii.guardGlyph() == "G");
    assert(ascii.switchGlyph(false) == "!" && ascii.switchGlyph(true) == "+");
}

void elTableroDibujaVigilanteEInterruptores() {
    NavigationEnvironment entorno(escenarioConVigilante());
    const ConsoleUI ui(RenderMode::ascii);

    auto pantalla = dibujar(ui, entorno);
    assert(glifoEn(pantalla, 1, 1, 2) == "@");     // jugador
    assert(glifoEn(pantalla, 1, 2, 2) == "!");     // A pendiente
    assert(glifoEn(pantalla, 1, 3, 2) == "!");     // B pendiente
    assert(glifoEn(pantalla, 2, 6, 2) == "G");     // vigilante
    assert(glifoEn(pantalla, 1, 5, 2) == "S");     // salida

    (void)entorno.step(Action::right);              // sobre A
    (void)entorno.step(Action::right);              // sobre B: A queda activado
    pantalla = dibujar(ui, entorno);
    assert(glifoEn(pantalla, 1, 2, 2) == "+");
    assert(glifoEn(pantalla, 1, 3, 2) == "@");
}

void elVigilanteSeDibujaSobreElJugadorCapturado() {
    Scenario escenario = escenarioConVigilante();
    escenario.guard_start = {1, 2};                 // pegado al jugador
    escenario.switch_a = {2, 1};
    NavigationEnvironment entorno(escenario);
    (void)entorno.step_player(Action::right);        // el jugador pisa al vigilante
    (void)entorno.finish_turn();
    assert(entorno.endReason() == EndReason::captured);
    const auto pantalla = dibujar(ConsoleUI(RenderMode::ascii), entorno);
    assert(glifoEn(pantalla, 1, 2, 2) == "G");
}

void laBarraIndicaCuandoSeMueveElVigilante() {
    NavigationEnvironment entorno(escenarioConVigilante());
    const ConsoleUI ui(RenderMode::ascii);

    auto texto = dibujar(ui, entorno).ToString();
    assert(texto.find("A pendiente | B pendiente") != std::string::npos);
    assert(texto.find("Vigilante: quieto este turno") != std::string::npos);   // el turno 1 es impar

    (void)entorno.step(Action::wait);
    texto = dibujar(ui, entorno).ToString();
    assert(texto.find("Vigilante: se mueve tras tu accion") != std::string::npos);  // el turno 2 es par
}

void sinVigilanteLaVistaNoCambia() {
    std::vector<std::string> lineas(20, std::string(30, '.'));
    lineas[0][0] = '@';
    lineas[19][29] = 'S';
    const auto escenario = parseScenario(lineas);
    const NavigationEnvironment entorno(escenario.grid, escenario.start);
    const auto texto = dibujar(ConsoleUI(RenderMode::ascii), entorno).ToString();
    assert(texto.find("Vigilante") == std::string::npos);
    assert(texto.find("pendiente") == std::string::npos);
}

// --------------------------------------------------- Terminal minima
void tamanoNecesarioSegunElTablero() {
    const auto tablero = [](std::size_t filas, std::size_t columnas) {
        std::vector<std::string> lineas(filas, std::string(columnas, '.'));
        lineas[0][0] = '@';
        lineas[filas - 1][columnas - 1] = 'S';
        return parseScenario(lineas);
    };
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        const ConsoleUI ui(modo);
        const auto grande = tablero(40, 60);
        const NavigationEnvironment g(grande.grid, grande.start);
        const auto tamano = ui.required_size(g);
        assert(tamano.dimx == 122 && tamano.dimy == 43);        // 2*(60+1) x (40+3)

        const auto medio = tablero(20, 30);
        const NavigationEnvironment m(medio.grid, medio.start);
        const auto tamanoMedio = ui.required_size(m);
        assert(tamanoMedio.dimx == 62 && tamanoMedio.dimy == 23);
    }
    // Con vigilante la vista tiene una linea mas.
    const NavigationEnvironment conVigilante(escenarioConVigilante());
    assert(ConsoleUI(RenderMode::ascii).required_size(conVigilante).dimy == 4 + 3 + 1);
}

// Zoom del tablero: cada casilla ocupa 2*zoom columnas y zoom filas.
void elZoomAgrandaElTablero() {
    std::vector<std::string> lineas(20, std::string(30, '.'));
    lineas[0][0] = '@';
    lineas[19][29] = 'S';
    const auto medio = parseScenario(lineas);
    const NavigationEnvironment m(medio.grid, medio.start);
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        ConsoleUI ui(modo);
        assert(ui.zoom() == 1);
        for (int z = 1; z <= ConsoleUI::max_zoom; ++z) {
            ui.setZoom(z);
            const auto tamano = ui.required_size(m);
            assert(tamano.dimx == 2 * z * (30 + 1));
            assert(tamano.dimy == 20 * z + 3);
        }
        ui.setZoom(0);
        assert(ui.zoom() == 1);
        ui.setZoom(99);
        assert(ui.zoom() == ConsoleUI::max_zoom);
    }
}

// El marco de la partida no cambia de tamano aunque cambien los textos: asi,
// centrado en la pantalla, no se desplaza al moverse el jugador.
void elMarcoDeLaPartidaTieneTamanoFijo() {
    const auto medida = [](const ftxui::Element& e) {
        e->ComputeRequirement();
        return std::pair{e->requirement().min_x, e->requirement().min_y};
    };
    for (const RenderMode modo : {RenderMode::ascii, RenderMode::emoji}) {
        for (int z = 1; z <= 3; ++z) {
            ConsoleUI ui(modo);
            ui.setZoom(z);
            NavigationEnvironment e(escenarioConVigilante());
            const std::vector<NavigationEvent> nada;
            const auto base = medida(ui.game_frame(e, nada, "Titulo", "teclas", ""));
            const std::string largo(300, 'x');  // aviso que ocupa mas de dos lineas
            assert(medida(ui.game_frame(e, nada, "Otro titulo mas largo", "teclas", largo)) == base);
            assert(medida(ui.game_frame(e, nada, "Titulo", largo, "corto")) == base);
            for (Action a : {Action::right, Action::wait, Action::up, Action::left, Action::down}) {
                if (e.isFinished()) break;
                const auto paso = e.step(a);
                assert(medida(ui.game_frame(e, paso.events, "Titulo", "teclas", "aviso")) == base);
            }
            // Ancho minimo para que entren las lineas de estado.
            assert(base.first >= ConsoleUI::frame_min_width);
        }
    }
}

void elMensajeDeTerminalPequenaDiceCuantoHaceFalta() {
    const ConsoleUI ui(RenderMode::ascii);
    auto mensaje = ui.too_small_message(ftxui::Dimensions{122, 43}, ftxui::Dimensions{80, 24});
    auto pantalla = ftxui::Screen::Create(ftxui::Dimension::Fit(mensaje));
    ftxui::Render(pantalla, mensaje);
    const std::string texto = pantalla.ToString();
    assert(texto.find("122x43") != std::string::npos);
    assert(texto.find("80x24") != std::string::npos);
    assert(texto.find("Q para salir") != std::string::npos);
}

void laAyudaListaVigilanteEInterruptores() {
    const ConsoleUI ui(RenderMode::ascii);
    auto ayuda = ui.help();
    // Pantalla fija: Dimension::Fit recorta a la terminal (24 filas) y la ayuda es mas larga.
    auto pantalla = ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(45));
    ftxui::Render(pantalla, ayuda);
    const std::string texto = pantalla.ToString();
    assert(texto.find("vigilante") != std::string::npos);
    assert(texto.find("interruptor pendiente") != std::string::npos);
    assert(texto.find("interruptor activado") != std::string::npos);
    assert(texto.find("Esc") != std::string::npos);
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
    tableroDeCualquierTamanoConservaFilasYColumnas();
    soloElTecladoCuentaComoComando();
    escenariosRealesSeDibujanEn20x30();
    laAyudaNoAvanzaTurnos();
    escapeSeTraduceAComandoDeSalida();
    glifosDelVigilanteYLosInterruptores();
    elTableroDibujaVigilanteEInterruptores();
    elVigilanteSeDibujaSobreElJugadorCapturado();
    laBarraIndicaCuandoSeMueveElVigilante();
    sinVigilanteLaVistaNoCambia();
    tamanoNecesarioSegunElTablero();
    elZoomAgrandaElTablero();
    elMarcoDeLaPartidaTieneTamanoFijo();
    elMensajeDeTerminalPequenaDiceCuantoHaceFalta();
    laAyudaListaVigilanteEInterruptores();
    return 0;
}