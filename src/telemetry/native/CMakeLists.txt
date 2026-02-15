cmake_minimum_required(VERSION 3.20)
project(aura_telemetry_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(aura_telemetry_native SHARED
    src/telemetry_engine.cpp
    src/telemetry_utils.cpp
    src/collect_cpu.cpp
    src/collect_processes.cpp
    src/collect_process_details.cpp
    src/collect_disk.cpp
    src/collect_network.cpp
    src/collect_gpu.cpp
    src/collect_thermal.cpp
    src/process_management.cpp
)

target_include_directories(aura_telemetry_native PUBLIC include)
target_link_libraries(aura_telemetry_native PRIVATE iphlpapi psapi)

set_target_properties(aura_telemetry_native PROPERTIES
    OUTPUT_NAME "aura_telemetry_native"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
