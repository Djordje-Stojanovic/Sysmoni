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

add_custom_command(
    TARGET aura_telemetry_native_tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:aura_telemetry_native>
        $<TARGET_FILE_DIR:aura_telemetry_native_tests>
)

add_test(
    NAME aura_telemetry_native_tests
    COMMAND $<TARGET_FILE:aura_telemetry_native_tests>
)

set_tests_properties(
    aura_telemetry_native_tests
    PROPERTIES
        WORKING_DIRECTORY $<TARGET_FILE_DIR:aura_telemetry_native_tests>
)

# Process ABI safety tests
add_executable(aura_process_abi_safety_tests
    native/test_process_abi_safety.cpp
)

target_include_directories(aura_process_abi_safety_tests PRIVATE
    "${TELEMETRY_NATIVE_DIR}/include"
)
target_link_libraries(aura_process_abi_safety_tests PRIVATE aura_telemetry_native)

add_custom_command(
    TARGET aura_process_abi_safety_tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:aura_telemetry_native>
        $<TARGET_FILE_DIR:aura_process_abi_safety_tests>
)

add_test(
    NAME aura_process_abi_safety_tests
    COMMAND $<TARGET_FILE:aura_process_abi_safety_tests>
)

set_tests_properties(
    aura_process_abi_safety_tests
    PROPERTIES
        WORKING_DIRECTORY $<TARGET_FILE_DIR:aura_process_abi_safety_tests>
)
