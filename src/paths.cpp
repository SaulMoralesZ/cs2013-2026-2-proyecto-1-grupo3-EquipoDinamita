#include "../include/circuit_escape/paths.hpp"

#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

#include "../include/circuit_escape/game_rules.hpp"
#include "../include/circuit_escape/ui/level_catalog.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace circuit_escape {

namespace fs = std::filesystem;

namespace {

std::optional<fs::path> env_path(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) return std::nullopt;
    return fs::path(value);
}

}  // namespace

fs::path executable_dir() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return {};
    return fs::path(std::wstring(buffer, length)).parent_path();
#else
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return {};
    return self.parent_path();
#endif
}

std::vector<fs::path> asset_dir_candidates() {
    std::vector<fs::path> candidates;
    if (const auto forced = env_path("CIRCUIT_ESCAPE_DATA")) candidates.push_back(*forced);
    const fs::path exe = executable_dir();
    if (!exe.empty()) {
        candidates.push_back(exe / "assets");
        candidates.push_back(exe.parent_path() / "share" / "escape_circuit" / "assets");
        candidates.push_back(exe.parent_path() / "assets");
    }
    candidates.push_back(fs::current_path() / "assets");  // ultimo recurso
    return candidates;
}

std::optional<fs::path> find_asset_dir() {
    std::error_code ec;
    for (const fs::path& dir : asset_dir_candidates()) {
        if (fs::is_directory(dir / "maps", ec)) return fs::weakly_canonical(dir, ec);
    }
    return std::nullopt;
}

fs::path user_data_dir() {
#if defined(_WIN32)
    if (const auto appdata = env_path("APPDATA")) return *appdata / "escape_circuit";
    return fs::current_path() / "escape_circuit_data";
#else
    if (const auto xdg = env_path("XDG_DATA_HOME")) return *xdg / "escape_circuit";
    if (const auto home = env_path("HOME")) return *home / ".local" / "share" / "escape_circuit";
    return fs::current_path() / ".escape_circuit";
#endif
}

fs::path default_saves_dir() { return user_data_dir() / "saves"; }

bool has_level_files(const fs::path& directory) {
    std::error_code ec;
    for (Difficulty level : {Difficulty::easy, Difficulty::standard, Difficulty::hard}) {
        if (!fs::is_regular_file(directory / level_file_name(level), ec)) return false;
    }
    return true;
}

fs::path default_maps_dir_for_reading() {
    const fs::path user = user_data_dir() / "maps";
    if (has_level_files(user)) return user;
    if (const auto assets = find_asset_dir()) return *assets / "maps";
    return user;
}

bool is_writable_dir(const fs::path& directory) {
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (!fs::is_directory(directory, ec)) return false;
    const fs::path probe = directory / ".escape_circuit_write_test";
    {
        std::ofstream file(probe);
        if (!file) return false;
    }
    fs::remove(probe, ec);
    return true;
}

fs::path default_maps_dir_for_writing() {
    if (const auto assets = find_asset_dir()) {
        if (is_writable_dir(*assets / "maps")) return *assets / "maps";
    }
    return user_data_dir() / "maps";
}

}  // namespace circuit_escape
