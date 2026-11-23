#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "circuit_escape/paths.hpp"
#include "circuit_escape/terminal_launcher.hpp"

using namespace circuit_escape;
namespace fs = std::filesystem;

namespace {

// Variables de entorno portables (setenv/unsetenv no existen en Windows).
void set_env(const char* name, const fs::path& value) {
#if defined(_WIN32)
    _putenv_s(name, value.string().c_str());
#else
    setenv(name, value.string().c_str(), 1);
#endif
}

void unset_env(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

// Carpeta base de los datos del usuario segun el sistema.
#if defined(_WIN32)
constexpr const char* kUserBaseVar = "APPDATA";
#else
constexpr const char* kUserBaseVar = "XDG_DATA_HOME";
#endif

// Simula los programas instalados: nombre -> ruta real.
auto installed(std::map<std::string, std::string> programs) {
    return [programs](const std::string& name) -> std::optional<std::string> {
        const auto it = programs.find(name);
        if (it == programs.end()) return std::nullopt;
        return it->second;
    };
}

const std::vector<std::string> kGame = {"/opt/juego/escape_circuit", "--launched"};

void execPrefixPerEmulator() {
    assert((terminal_exec_prefix("/usr/bin/ptyxis") == std::vector<std::string>{"--"}));
    assert((terminal_exec_prefix("gnome-terminal") == std::vector<std::string>{"--"}));
    assert((terminal_exec_prefix("/usr/bin/xfce4-terminal") == std::vector<std::string>{"-x"}));
    assert((terminal_exec_prefix("wezterm") == std::vector<std::string>{"start", "--"}));
    assert(terminal_exec_prefix("/usr/bin/kitty").empty());
    assert((terminal_exec_prefix("/usr/bin/konsole") == std::vector<std::string>{"-e"}));
    assert((terminal_exec_prefix("xterm") == std::vector<std::string>{"-e"}));
    assert((terminal_exec_prefix("una-terminal-rara") == std::vector<std::string>{"-e"}));
}

void preferredFirstThenSystemThenKnown() {
    const auto candidates = terminal_candidates(
        kGame, std::string("alacritty"),
        installed({{"alacritty", "/usr/bin/alacritty"},
                   {"x-terminal-emulator", "/usr/bin/ptyxis"},  // enlace ya resuelto
                   {"ptyxis", "/usr/bin/ptyxis"},
                   {"xterm", "/usr/bin/xterm"}}));
    assert(candidates.size() == 3);  // ptyxis aparece una sola vez
    assert(candidates[0].program == "/usr/bin/alacritty");
    assert((candidates[0].args ==
            std::vector<std::string>{"-e", "/opt/juego/escape_circuit", "--launched"}));
    // x-terminal-emulator se usa con las opciones del programa real (ptyxis -> "--").
    assert(candidates[1].program == "/usr/bin/ptyxis");
    assert((candidates[1].args ==
            std::vector<std::string>{"--", "/opt/juego/escape_circuit", "--launched"}));
    assert(candidates[2].program == "/usr/bin/xterm");
}

void preferredNotInstalledIsSkipped() {
    // Como en un equipo con TERMINAL=ghostty pero sin ghostty instalado.
    const auto candidates = terminal_candidates(
        kGame, std::string("ghostty"), installed({{"konsole", "/usr/bin/konsole"}}));
    assert(candidates.size() == 1);
    assert(candidates[0].program == "/usr/bin/konsole");
}

void noTerminalAtAll() {
    assert(terminal_candidates(kGame, std::nullopt, installed({})).empty());
}

void findInPathFindsRealPrograms() {
#if defined(_WIN32)
    assert(find_in_path("cmd.exe").has_value());
#else
    assert(find_in_path("sh").has_value());
#endif
    assert(!find_in_path("programa-que-no-existe-12345").has_value());
}

// ------------------------------------------------------------------ Rutas
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / ("circuit_escape_" + name)) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

void touchLevels(const fs::path& dir) {
    fs::create_directories(dir);
    for (const char* name : {"mapa_facil.txt", "mapa_medio.txt", "mapa_dificil.txt"}) {
        std::ofstream(dir / name) << "x\n";
    }
}

void assetsAreRelativeToTheExecutable() {
    const fs::path exe = executable_dir();
    assert(!exe.empty());
    const auto candidates = asset_dir_candidates();
    assert(std::find(candidates.begin(), candidates.end(), exe / "assets") != candidates.end());
    assert(std::find(candidates.begin(), candidates.end(),
                     exe.parent_path() / "share" / "escape_circuit" / "assets") !=
           candidates.end());
    // La prueba corre desde build/: encuentra los recursos del repositorio.
    const auto found = find_asset_dir();
    assert(found && fs::is_directory(*found / "maps"));
}

void forcedDataDirAndUserDir() {
    TempDir data("forced_data");
    TempDir user("user_data");
    fs::create_directories(data.path / "maps");
    set_env("CIRCUIT_ESCAPE_DATA", data.path);
    set_env(kUserBaseVar, user.path);

    assert(find_asset_dir() == fs::weakly_canonical(data.path));
    assert(user_data_dir() == user.path / "escape_circuit");
    assert(default_saves_dir() == user.path / "escape_circuit" / "saves");

    // Sin niveles del usuario se leen los incluidos.
    assert(default_maps_dir_for_reading() == fs::weakly_canonical(data.path) / "maps");
    // Los incluidos se pueden escribir (carpeta temporal): el generador escribe ahi.
    assert(default_maps_dir_for_writing() == fs::weakly_canonical(data.path) / "maps");

    // Con los tres niveles del usuario, el juego prefiere los del usuario.
    touchLevels(user.path / "escape_circuit" / "maps");
    assert(has_level_files(user.path / "escape_circuit" / "maps"));
    assert(default_maps_dir_for_reading() == user.path / "escape_circuit" / "maps");

    assert(!has_level_files(data.path / "maps"));
    assert(is_writable_dir(user.path / "nueva" / "carpeta"));

    unset_env("CIRCUIT_ESCAPE_DATA");
    unset_env(kUserBaseVar);
}

}  // namespace

int main() {
    execPrefixPerEmulator();
    preferredFirstThenSystemThenKnown();
    preferredNotInstalledIsSkipped();
    noTerminalAtAll();
    findInPathFindsRealPrograms();
    assetsAreRelativeToTheExecutable();
    forcedDataDirAndUserDir();
    return 0;
}
