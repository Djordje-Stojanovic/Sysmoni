cmake_minimum_required(VERSION 3.23)
project(aura_render_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(../../src/render/native ${CMAKE_BINARY_DIR}/render-native)

add_executable(render_native_tests render_native_tests.cpp)
target_include_directories(render_native_tests PRIVATE ../../src/render/native/include)
target_link_libraries(render_native_tests PRIVATE aura_render_native)
add_custom_command(
    TARGET render_native_tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:aura_render_native>
        $<TARGET_FILE_DIR:render_native_tests>
)

enable_testing()
add_test(NAME render_native_tests COMMAND render_native_tests)
