cmake_minimum_required(VERSION 3.26)

project(aura_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AURA_SHELL_BUILD_APP "Build the Qt shell executable target." ON)

find_package(Qt6 6.5 COMPONENTS Widgets QuickWidgets Qml QUIET)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
endif()

set(AURA_SHELL_CORE_SOURCES
    src/cockpit_controller.cpp
    src/cockpit_controller_support.cpp
    src/dock_model.cpp
    src/analytics_bridge.cpp
    src/persistence_bridge.cpp
    src/render_bridge.cpp
    src/telemetry_bridge.cpp
    src/timeline_bridge.cpp
    src/ui_theme.cpp
    src/size_metrics.cpp
)
if(Qt6_FOUND)
    list(APPEND AURA_SHELL_CORE_SOURCES
        src/process_list_model.cpp
        include/aura_shell/process_list_model.hpp
    )
endif()

add_library(aura_shell_core STATIC ${AURA_SHELL_CORE_SOURCES})
target_include_directories(aura_shell_core
    PUBLIC include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../telemetry/native/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../render/native/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/native/include
)
if(Qt6_FOUND)
    target_link_libraries(aura_shell_core
        PUBLIC Qt6::Widgets Qt6::Qml
    )
endif()

if(AURA_SHELL_BUILD_APP)
    if(Qt6_FOUND)
        add_executable(aura_shell
            src/main.cpp
            src/shell_utils.cpp
            src/stylesheet_builder.cpp
            src/aura_shell_window.cpp
            src/aura_shell_window_panels.cpp
            src/aura_shell_window_refresh.cpp
        )
        target_compile_definitions(aura_shell PRIVATE
            AURA_SHELL_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/CockpitScene.qml"
            AURA_SHELL_TIMELINE_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/TimelineScene.qml"
            AURA_SHELL_PROCESS_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/ProcessScene.qml"
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
