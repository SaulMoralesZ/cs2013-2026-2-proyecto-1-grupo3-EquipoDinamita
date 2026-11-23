#include "../include/circuit_escape/terminal_launcher.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../include/circuit_escape/paths.hpp"

namespace circuit_escape {

namespace fs = std::filesystem;

bool running_in_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

std::vector<std::string> terminal_exec_prefix(const std::string& emulator) {
    const std::string name = fs::path(emulator).filename().string();
    if (name == "gnome-terminal" || name == "ptyxis") return {"--"};
    if (name == "xfce4-terminal" || name == "mate-terminal" || name == "terminator") {
        return {"-x"};
    }
    if (name == "wezterm") return {"start", "--"};
    if (name == "kitty" || name == "foot") return {};  // el comando va directo
    return {"-e"};  // konsole, xterm, alacritty, ghostty, kgx, tilix, lxterminal...
}

std::vector<TerminalCommand> terminal_candidates(
    const std::vector<std::string>& command, const std::optional<std::string>& preferred,
    const std::function<std::optional<std::string>(const std::string&)>& resolve) {
    std::vector<std::string> names;
    if (preferred && !preferred->empty()) names.push_back(*preferred);
    names.push_back("x-terminal-emulator");  // alternativa del sistema (Debian/Ubuntu)
    for (const char* known : {"ptyxis", "gnome-terminal", "kgx", "konsole", "xfce4-terminal",
                              "mate-terminal", "tilix", "terminator", "lxterminal", "alacritty",
                              "kitty", "wezterm", "foot", "ghostty", "xterm"}) {
        names.emplace_back(known);
    }

    std::vector<TerminalCommand> candidates;
    std::set<std::string> seen;
    for (const std::string& name : names) {
        const auto path = resolve(name);
        if (!path || !seen.insert(*path).second) continue;
        TerminalCommand candidate{*path, terminal_exec_prefix(*path)};
        candidate.args.insert(candidate.args.end(), command.begin(), command.end());
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

std::optional<std::string> find_in_path(const std::string& program) {
    std::error_code ec;
    const auto real = [&](const fs::path& p) -> std::optional<std::string> {
        if (!fs::is_regular_file(p, ec)) return std::nullopt;
        return fs::canonical(p, ec).string();
    };
    if (program.find('/') != std::string::npos) return real(program);
    const char* path = std::getenv("PATH");
    if (!path) return std::nullopt;
    const std::string dirs = path;
#if defined(_WIN32)
    const char separator = ';';
#else
    const char separator = ':';
#endif
    std::size_t start = 0;
    while (start <= dirs.size()) {
        const std::size_t end = dirs.find(separator, start);
        const std::string dir = dirs.substr(start, end == std::string::npos ? std::string::npos
                                                                            : end - start);
        if (!dir.empty()) {
            if (auto found = real(fs::path(dir) / program)) return found;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return std::nullopt;
}

bool relaunch_in_terminal(const std::vector<std::string>& args) {
#if defined(_WIN32)
    (void)args;
    return false;
#else
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return false;

    std::vector<std::string> command{self.string()};
    command.insert(command.end(), args.begin(), args.end());
    command.emplace_back(kLaunchedFlag);

    const char* preferred = std::getenv("TERMINAL");
    for (const TerminalCommand& terminal :
         terminal_candidates(command,
                             preferred ? std::optional<std::string>(preferred) : std::nullopt,
                             find_in_path)) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(terminal.program.c_str()));
        for (const std::string& a : terminal.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(terminal.program.c_str(), argv.data());  // solo vuelve si fallo
    }
    return false;
#endif
}

void prepare_console() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

bool console_supports_emoji() {
#if defined(_WIN32)
    // Windows Terminal define WT_SESSION; la consola clasica no muestra emojis.
    return std::getenv("WT_SESSION") != nullptr;
#else
    return true;
#endif
}

void pause_before_closing(bool launchedWindow) {
#if defined(_WIN32)
    // Doble clic en el .exe: la consola es solo de este proceso.
    DWORD processes[2];
    launchedWindow = launchedWindow || GetConsoleProcessList(processes, 2) == 1;
#endif
    if (!launchedWindow) return;
    std::cerr << "\nPulsa Enter para cerrar esta ventana...";
    std::string line;
    std::getline(std::cin, line);
}

}  // namespace circuit_escape
