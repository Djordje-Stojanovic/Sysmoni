cmake_minimum_required(VERSION 3.26)
project(aura_shell_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AURA_SHELL_BUILD_APP OFF CACHE BOOL "" FORCE)
if(NOT TARGET aura_shell_core)
    add_subdirectory(../../src/shell/native ${CMAKE_BINARY_DIR}/shell-native)
endif()

enable_testing()

add_executable(aura_shell_dock_model_tests
    native/dock_model_tests.cpp
)
target_link_libraries(aura_shell_dock_model_tests PRIVATE aura_shell_core)
add_test(NAME aura_shell_dock_model_tests COMMAND aura_shell_dock_model_tests)

add_executable(aura_shell_cockpit_controller_tests
    native/cockpit_controller_tests.cpp
)
target_link_libraries(aura_shell_cockpit_controller_tests PRIVATE aura_shell_core)
add_test(NAME aura_shell_cockpit_controller_tests COMMAND aura_shell_cockpit_controller_tests)

add_executable(aura_shell_timeline_bridge_tests
    native/timeline_bridge_tests.cpp
)
target_link_libraries(aura_shell_timeline_bridge_tests PRIVATE aura_shell_core)
add_test(NAME aura_shell_timeline_bridge_tests COMMAND aura_shell_timeline_bridge_tests)
