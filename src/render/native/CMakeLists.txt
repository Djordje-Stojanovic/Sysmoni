cmake_minimum_required(VERSION 3.23)
project(aura_render_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RENDER_NATIVE_ENABLE_QT_WIDGETS "Enable Qt Widgets + RHI native backend hooks." OFF)

set(RENDER_NATIVE_SOURCES
    src/c_api.cpp
    src/math.cpp
    src/theme.cpp
    src/formatting.cpp
    src/status.cpp
    src/widgets.cpp
    src/widgets_models.cpp
)

add_library(aura_render_native SHARED ${RENDER_NATIVE_SOURCES})
target_include_directories(aura_render_native PUBLIC include)
target_compile_definitions(aura_render_native PRIVATE AURA_RENDER_NATIVE_EXPORTS=1)

if(RENDER_NATIVE_ENABLE_QT_WIDGETS)
    find_package(Qt6 COMPONENTS Widgets Gui OpenGL OpenGLWidgets QUIET)
    if(Qt6_FOUND)
        target_compile_definitions(aura_render_native PRIVATE RENDER_NATIVE_QT_WIDGETS=1)
        target_link_libraries(aura_render_native PRIVATE Qt6::Widgets Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets)
    endif()
endif()
