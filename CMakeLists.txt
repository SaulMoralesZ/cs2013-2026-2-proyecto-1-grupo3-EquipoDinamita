cmake_minimum_required(VERSION 3.20)
project(escape_circuit_suite
        VERSION 1.0.0
        DESCRIPTION "Circuito de Escape: laberinto por turnos con vigilante, Kruskal, Dijkstra y BFS"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Para clangd / VS Code: build/compile_commands.json con los include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(CIRCUIT_ESCAPE_WITH_UI "Construye el ejecutable de consola con FTXUI" ON)
# Ejecutables sin dependencias del sistema (ni glibc): el paquete funciona en
# distribuciones mas antiguas que la de compilacion. Lo activa installer/build_packages.sh.
option(CIRCUIT_ESCAPE_STATIC "Enlaza los ejecutables de forma estatica (paquetes portables)" OFF)

# MSVC: con CIRCUIT_ESCAPE_STATIC se usa el runtime estatico (/MT), asi el .exe
# no necesita el redistribuible de Visual C++. Debe fijarse antes de crear
# los targets (incluidos los de FTXUI).
if(MSVC AND CIRCUIT_ESCAPE_STATIC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CIRCUIT_ESCAPE_RUNTIME_DLLS "")
if(MINGW)
    foreach(dll IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        execute_process(
                COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=${dll}
                OUTPUT_VARIABLE dll_path
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(IS_ABSOLUTE "${dll_path}" AND EXISTS "${dll_path}")
            list(APPEND CIRCUIT_ESCAPE_RUNTIME_DLLS "${dll_path}")
        endif()
    endforeach()
endif()

function(circuit_escape_configure target)
    if(MSVC)
        # /utf-8: los fuentes y los textos (emojis, bordes) son UTF-8.
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    get_target_property(target_type ${target} TYPE)
    if(MINGW AND target_type STREQUAL "EXECUTABLE" AND CIRCUIT_ESCAPE_RUNTIME_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CIRCUIT_ESCAPE_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:${target}>"
                VERBATIM
        )
    endif()
endfunction()

# Biblioteca compartida por el juego y el generador de mapas.
add_library(escape_core STATIC
        src/position.cpp
        src/environment.cpp
        src/controllers.cpp
        src/controllers/guard_controller.cpp
        src/game_turn.cpp
        src/graph/graph.cpp
        src/pathfinding/dijkstra.cpp
        src/pathfinding/bfs.cpp
        src/pathfinding/route_planner.cpp
        src/map_generation/map_builder.cpp
        src/map_generation/map_validator.cpp
        src/map_generation/winning_play.cpp
        src/map_generation/map_selector.cpp
        src/map_generation/map_batch_runner.cpp
        src/ui/energy_help.cpp
        src/ui/level_catalog.cpp
        src/replay_session.cpp
        src/game_record.cpp
        src/paths.cpp
        src/terminal_launcher.cpp
        src/map_io/scenario_io.cpp
)
target_include_directories(escape_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(escape_core PUBLIC Threads::Threads)
circuit_escape_configure(escape_core)

# Generador de mapas: no depende de la interfaz.
add_executable(map_generator app/map_generator_main.cpp)
target_link_libraries(map_generator PRIVATE escape_core)
circuit_escape_configure(map_generator)

if(CIRCUIT_ESCAPE_WITH_UI)
    include(FetchContent)
    FetchContent_Declare(ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
            GIT_TAG v7.0.3
            FIND_PACKAGE_ARGS 7.0.3
    )
    # Sin las reglas de instalacion de FTXUI: si no, sus cabeceras y bibliotecas
    # entran en los paquetes y chocan con el paquete ftxui del sistema.
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ftxui)

    add_library(circuit_escape_ui STATIC src/console_ui.cpp src/ui/game_application.cpp)
    target_link_libraries(circuit_escape_ui PUBLIC
            escape_core
            ftxui::screen
            ftxui::dom
            ftxui::component
    )
    circuit_escape_configure(circuit_escape_ui)
    # GCC 12+ en Release da falsos positivos -Warray-bounds dentro de
    # shared_ptr al optimizar los nodos de FTXUI (no es codigo del proyecto).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(circuit_escape_ui PRIVATE -Wno-array-bounds)
    endif()

    add_executable(escape_circuit app/main.cpp)
    target_link_libraries(escape_circuit PRIVATE circuit_escape_ui)
    circuit_escape_configure(escape_circuit)
endif()

if(CIRCUIT_ESCAPE_STATIC AND NOT MSVC)
    target_link_options(map_generator PRIVATE -static)
    if(CIRCUIT_ESCAPE_WITH_UI)
        target_link_options(escape_circuit PRIVATE -static)
    endif()
endif()

# ------------------------------------------------------------- Instalacion
# El juego busca los mapas relativos al ejecutable (src/paths.cpp):
#   Linux (.deb y .tar.gz)            Windows (instalador y .zip)
#     bin/escape_circuit                escape_circuit.exe
#     bin/map_generator                 map_generator.exe
#     share/escape_circuit/assets/maps  assets/maps
#     share/applications/  (acceso)     LEEME.txt, README.md
#     share/icons/...      (icono)
set(CMAKE_INSTALL_DOCDIR "share/doc/escape-circuit")
include(GNUInstallDirs)
if(WIN32)
    set(CIRCUIT_ESCAPE_BIN_DIR ".")
    set(CIRCUIT_ESCAPE_ASSET_DIR "assets")
    set(CIRCUIT_ESCAPE_DOC_DIR ".")
else()
    set(CIRCUIT_ESCAPE_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
    set(CIRCUIT_ESCAPE_ASSET_DIR "${CMAKE_INSTALL_DATADIR}/escape_circuit/assets")
    set(CIRCUIT_ESCAPE_DOC_DIR "${CMAKE_INSTALL_DOCDIR}")
endif()

install(TARGETS map_generator RUNTIME DESTINATION ${CIRCUIT_ESCAPE_BIN_DIR})
if(CIRCUIT_ESCAPE_WITH_UI)
    install(TARGETS escape_circuit RUNTIME DESTINATION ${CIRCUIT_ESCAPE_BIN_DIR})
endif()
# Solo el lote por nivel y los mapas del Proyecto 1 (no los mapa_N sueltos).
install(DIRECTORY assets/maps/ DESTINATION ${CIRCUIT_ESCAPE_ASSET_DIR}/maps
        FILES_MATCHING
        PATTERN "mapa_facil.txt" PATTERN "mapa_medio.txt" PATTERN "mapa_dificil.txt"
        PATTERN "scenario_*.txt")
if(UNIX AND NOT APPLE)
    install(FILES installer/linux/escape-circuit.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES installer/linux/escape-circuit.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
if(WIN32)
    install(FILES installer/windows/LEEME.txt DESTINATION ${CIRCUIT_ESCAPE_DOC_DIR})
endif()
install(FILES README.md DESTINATION ${CIRCUIT_ESCAPE_DOC_DIR})

# --------------------------------------------------------------- Paquetes
# Linux:   cpack -> .deb (instalador) y .tar.gz (portable)
# Windows: cpack -> instalador NSIS (.exe) y .zip (portable)
set(CPACK_PACKAGE_NAME "escape-circuit")
set(CPACK_PACKAGE_VENDOR "Equipo Dinamita - CS2013 UTEC")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_STRIP_FILES ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)  # portable: una carpeta con todo

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "escape-circuit-${PROJECT_VERSION}-windows-x86_64")
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Circuito de Escape")
    # Acceso en el menu Inicio y en el escritorio: abre el juego en su consola.
    set(CPACK_PACKAGE_EXECUTABLES "escape_circuit;Circuito de Escape")
    set(CPACK_CREATE_DESKTOP_LINKS "escape_circuit")
    set(CPACK_NSIS_DISPLAY_NAME "Circuito de Escape")
    set(CPACK_NSIS_PACKAGE_NAME "Circuito de Escape ${PROJECT_VERSION}")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "escape_circuit.exe")
else()
    set(CPACK_PACKAGE_FILE_NAME "escape-circuit-${PROJECT_VERSION}-linux-x86_64")
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Equipo Dinamita - CS2013 UTEC")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Activa los interruptores A y B y escapa sin que te atrape el vigilante.\n\
Incluye escape_circuit (juego) y map_generator (preparacion concurrente de mapas).")
    if(CIRCUIT_ESCAPE_STATIC)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "")  # ejecutables estaticos: sin dependencias
    else()
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    endif()
endif()
include(CPack)

enable_testing()

set(CIRCUIT_ESCAPE_TESTS
        grid_test
        environment_test
        interactions_test
        controllers_test
        graph_test
        map_builder_test
        observation_test
        scenario_test
        turn_test
)

foreach(test_name IN LISTS CIRCUIT_ESCAPE_TESTS)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE escape_core)
    circuit_escape_configure(${test_name})
    add_test(NAME ${test_name}
             COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# Pruebas en subcarpetas (seccion 5.2): tests/<carpeta>/<nombre>.cpp
set(CIRCUIT_ESCAPE_SUBDIR_TESTS
        pathfinding/pathfinding_test
        controllers/guard_controller_test
        map_generation/map_validator_test
        map_generation/winning_play_test
        map_generation/map_selector_test
        concurrency/map_batch_test
        ui/energy_help_test
        ui/level_catalog_test
        ui/game_record_test
        ui/launch_test
)
foreach(test_path IN LISTS CIRCUIT_ESCAPE_SUBDIR_TESTS)
    get_filename_component(test_name ${test_path} NAME)
    add_executable(${test_name} tests/${test_path}.cpp)
    target_link_libraries(${test_name} PRIVATE escape_core)
    circuit_escape_configure(${test_name})
    add_test(NAME ${test_name}
             COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# Programa de demostración del entorno (no es un test de CTest)
add_executable(demo_environment tests/test1-1.cpp)
target_link_libraries(demo_environment PRIVATE escape_core)
circuit_escape_configure(demo_environment)

# Prueba de la interfaz
if(CIRCUIT_ESCAPE_WITH_UI)
    add_executable(ui_test tests/ui_test.cpp)
    target_link_libraries(ui_test PRIVATE circuit_escape_ui)
    circuit_escape_configure(ui_test)
    add_test(NAME ui_test
            COMMAND ui_test
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
