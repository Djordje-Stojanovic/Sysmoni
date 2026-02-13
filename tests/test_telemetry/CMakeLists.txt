cmake_minimum_required(VERSION 3.20)
project(aura_telemetry_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(TELEMETRY_NATIVE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../src/telemetry/native")
add_subdirectory(
    "${TELEMETRY_NATIVE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/telemetry_native"
)

add_executable(aura_telemetry_native_tests
    native/test_telemetry_native.cpp
)

target_include_directories(aura_telemetry_native_tests PRIVATE
    "${TELEMETRY_NATIVE_DIR}/include"
)
target_link_libraries(aura_telemetry_native_tests PRIVATE aura_telemetry_native)

add_test(
    NAME aura_telemetry_native_tests
    COMMAND aura_telemetry_native_tests
)
