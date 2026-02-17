cmake_minimum_required(VERSION 3.21)

project(aura_platform_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_OMIT_LOAD_EXTENSION=1
)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

set(AURA_NATIVE_SOURCES
    src/c_api.cpp
    src/c_api_alert.cpp
    src/c_api_stats.cpp
    src/c_api_health.cpp
    src/config_win.cpp
    src/store_sqlite.cpp
    src/dvr_lttb.cpp
    src/stats_engine.cpp
    src/dvr_export.cpp
    src/system_snapshot_win.cpp
    src/alert_engine.cpp
    src/health_engine.cpp
)

add_library(aura_platform SHARED ${AURA_NATIVE_SOURCES})
target_include_directories(aura_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(aura_platform PRIVATE sqlite3)

target_compile_definitions(aura_platform PRIVATE AURA_PLATFORM_BUILD_DLL=1)

if(MSVC)
    target_compile_options(aura_platform PRIVATE /W4 /EHsc)
else()
    target_compile_options(aura_platform PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(aura src/runtime_cli.cpp)
target_include_directories(aura
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(aura PRIVATE aura_platform)

set_target_properties(aura_platform PROPERTIES
    OUTPUT_NAME "aura_platform"
)
set_target_properties(aura PROPERTIES
    OUTPUT_NAME "aura"
)

install(TARGETS aura aura_platform
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
