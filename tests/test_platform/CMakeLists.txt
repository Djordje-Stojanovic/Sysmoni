cmake_minimum_required(VERSION 3.21)

project(aura_platform_tests LANGUAGES CXX)

enable_testing()

add_subdirectory(../../src/runtime/native runtime_native)
add_subdirectory(native)
