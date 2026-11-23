#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace circuit_escape {

// Arranque con doble clic (sin escribir comandos).
//  - Linux: si el juego se abre sin terminal (gestor de archivos, menu de
//    aplicaciones), se vuelve a ejecutar dentro de una terminal instalada.
//  - Windows: un .exe de consola ya abre su ventana; se activa UTF-8 y, si
//    la consola no es Windows Terminal, se usa el modo ASCII.

// Opcion interna con la que el juego se relanza a si mismo en la terminal.
inline constexpr const char* kLaunchedFlag = "--launched";

// true si la entrada, la salida o la salida de errores es una terminal.
// Solo con doble clic (ninguna conectada) conviene abrir una terminal nueva.
[[nodiscard]] bool running_in_terminal();

// Una forma concreta de abrir `command` dentro de un emulador de terminal.
struct TerminalCommand {
    std::string program;            // ruta del emulador
    std::vector<std::string> args;  // argumentos completos, incluido el comando
};

// Argumentos que necesita cada emulador antes del comando ("-e", "--", "-x"...),
// segun el nombre del programa. Desconocido -> "-e" (convencion habitual).
[[nodiscard]] std::vector<std::string> terminal_exec_prefix(const std::string& emulator);

// Candidatos en orden: $TERMINAL, x-terminal-emulator (resuelto al programa
// real) y una lista de emuladores conocidos. `resolve` devuelve la ruta real
// de un programa del PATH o nullopt si no esta instalado. Sin duplicados.
[[nodiscard]] std::vector<TerminalCommand> terminal_candidates(
    const std::vector<std::string>& command, const std::optional<std::string>& preferred,
    const std::function<std::optional<std::string>(const std::string&)>& resolve);

// Busca `program` en el PATH y devuelve su ruta real (enlaces resueltos).
[[nodiscard]] std::optional<std::string> find_in_path(const std::string& program);

// Linux: reemplaza el proceso por una terminal que ejecuta este mismo
// programa con `args` + kLaunchedFlag. Solo vuelve si ninguna terminal
// pudo abrirse (devuelve false). En Windows no hace nada.
bool relaunch_in_terminal(const std::vector<std::string>& args);

// Windows: consola en UTF-8. En Linux no hace nada.
void prepare_console();

// false solo en la consola clasica de Windows (sin emojis): conviene ASCII.
[[nodiscard]] bool console_supports_emoji();

// Al salir con error en una ventana abierta solo para el juego, espera una
// tecla para que el mensaje se pueda leer antes de que se cierre.
void pause_before_closing(bool launchedWindow);

}  // namespace circuit_escape
