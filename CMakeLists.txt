cmake_minimum_required(VERSION 3.16)
project(circuit_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


option(CIRCUIT_ESCAPE_WITH_UI "Construye el ejecutable de consola con FTXUI" ON)

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
        target_compile_options(${target} PRIVATE /W4)
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

add_library(circuit_escape STATIC
        src/position.cpp
        src/environment.cpp
        src/controllers.cpp
)
target_include_directories(circuit_escape PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
circuit_escape_configure(circuit_escape)

if(CIRCUIT_ESCAPE_WITH_UI)
    include(FetchContent)
    FetchContent_Declare(ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
            GIT_TAG v7.0.3
    )
    FetchContent_MakeAvailable(ftxui)

    add_library(circuit_escape_ui STATIC src/console_ui.cpp)
    target_link_libraries(circuit_escape_ui PUBLIC
            circuit_escape
            ftxui::screen
            ftxui::dom
            ftxui::component
    )
    circuit_escape_configure(circuit_escape_ui)

    add_executable(navigation_game app/main.cpp)
    target_link_libraries(navigation_game PRIVATE circuit_escape_ui)
    circuit_escape_configure(navigation_game)
endif()

enable_testing()

set(CIRCUIT_ESCAPE_TESTS
        grid_test
        environment_test
        interactions_test
        controllers_test
)

foreach(test_name IN LISTS CIRCUIT_ESCAPE_TESTS)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE circuit_escape)
    circuit_escape_configure(${test_name})
    add_test(NAME ${test_name}
            COMMAND ${test_name}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()