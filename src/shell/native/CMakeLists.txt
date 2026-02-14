cmake_minimum_required(VERSION 3.26)

project(aura_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AURA_SHELL_BUILD_APP "Build the Qt shell executable target." ON)

enable_testing()

add_library(aura_shell_core STATIC
    src/dock_model.cpp
)
target_include_directories(aura_shell_core PUBLIC include)

if(AURA_SHELL_BUILD_APP)
    find_package(Qt6 6.5 COMPONENTS Widgets QuickWidgets Qml QUIET)
    if(Qt6_FOUND)
        add_executable(aura_shell
            src/main.cpp
        )
        target_compile_definitions(aura_shell PRIVATE
            AURA_SHELL_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/CockpitScene.qml"
        )
        target_link_libraries(aura_shell PRIVATE
            aura_shell_core
            Qt6::Widgets
            Qt6::QuickWidgets
            Qt6::Qml
        )
    else()
        message(WARNING "Qt6 not found. Building core + tests only. Set CMAKE_PREFIX_PATH or Qt6_DIR to build aura_shell.exe.")
    endif()
endif()

add_executable(aura_shell_dock_model_tests
    tests/dock_model_tests.cpp
)
target_link_libraries(aura_shell_dock_model_tests PRIVATE aura_shell_core)
add_test(NAME aura_shell_dock_model_tests COMMAND aura_shell_dock_model_tests)
