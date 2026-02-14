cmake_minimum_required(VERSION 3.21)

project(aura_platform_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(../../src/runtime/native runtime_native)
add_subdirectory(native)
