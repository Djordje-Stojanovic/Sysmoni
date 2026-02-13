cmake_minimum_required(VERSION 3.23)
project(aura_render_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(render_native_tests render_native_tests.cpp)
target_include_directories(render_native_tests PRIVATE ../../src/render/native/include)
target_link_directories(render_native_tests PRIVATE ../../build/render-native)
target_link_libraries(render_native_tests PRIVATE aura_render_native)

enable_testing()
add_test(NAME render_native_tests COMMAND render_native_tests)
