cmake_minimum_required(VERSION 3.26)

project(aura_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets QuickWidgets Qml)

enable_testing()

add_library(aura_shell_core STATIC
    src/dock_model.cpp
)
target_include_directories(aura_shell_core PUBLIC include)

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

add_executable(aura_shell_dock_model_tests
    tests/dock_model_tests.cpp
)
target_link_libraries(aura_shell_dock_model_tests PRIVATE aura_shell_core)
add_test(NAME aura_shell_dock_model_tests COMMAND aura_shell_dock_model_tests)

