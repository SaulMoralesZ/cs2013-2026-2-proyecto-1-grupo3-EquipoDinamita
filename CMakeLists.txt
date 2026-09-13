cmake_minimum_required(VERSION 3.10)
project(EquipoDinamita CXX)

# Estándar de C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flags para compiladores
if(MSVC)
    # Visual Studio en Windows
    add_compile_options(/W4)
else()
    # GCC/Clang en Linux/Mac
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# FetchContent para descargar FTXUI automáticamente
include(FetchContent)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v7.0.3
)
FetchContent_MakeAvailable(ftxui)

# Agrupa archivos fuente
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# Crea el ejecutable
add_executable(proyecto ${SOURCES})

# Include directories
target_include_directories(proyecto PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Vincula FTXUI
target_link_libraries(proyecto PRIVATE ftxui::ftxui)
