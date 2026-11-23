# Toolchain de CMake para compilar ejecutables de Windows desde Linux (MinGW-w64).
#   sudo apt install mingw-w64 nsis
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=installer/windows/mingw-w64-x86_64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Variante "posix" de MinGW: necesaria para std::thread / std::jthread.
find_program(CIRCUIT_ESCAPE_MINGW_CXX NAMES x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-g++)
find_program(CIRCUIT_ESCAPE_MINGW_RC NAMES x86_64-w64-mingw32-windres)
set(CMAKE_CXX_COMPILER ${CIRCUIT_ESCAPE_MINGW_CXX})
set(CMAKE_RC_COMPILER ${CIRCUIT_ESCAPE_MINGW_RC})

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
