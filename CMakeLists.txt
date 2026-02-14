# ===========================================================================
# Aura System Monitor -- Root Superbuild
# ===========================================================================
#
# Single-command build for all 4 native modules:
#
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release
#   ctest --test-dir build -C Release --output-on-failure
#
# Qt6 is optional. If not found, the SHELL app target is skipped and the
# RENDER module builds without Qt widget hooks. Everything else builds fine.
#
# To point CMake at a Qt6 installation:
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
#         -DQt6_DIR="C:/Qt/6.10.2/msvc2022_64/lib/cmake/Qt6"
# ===========================================================================

cmake_minimum_required(VERSION 3.23)

project(aura
    VERSION 0.1.0
    DESCRIPTION "Aura -- native Windows system monitor"
    LANGUAGES CXX
)

# ---- Global defaults ----------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Place all build artifacts in a predictable location so DLLs are next to
# executables and tests can find them without post-build copy steps.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---- Options ------------------------------------------------------------

option(AURA_BUILD_TESTS "Build test executables for all modules" ON)

# ---- Module: PLATFORM (runtime, config, DVR, store) ---------------------

add_subdirectory(src/runtime/native)

# ---- Module: SENSOR (telemetry collection) ------------------------------

add_subdirectory(src/telemetry/native)

# ---- Module: RENDER (visual primitives, widgets, compositor) ------------

add_subdirectory(src/render/native)

# ---- Module: SHELL (Qt6 frameless window, docking, QML cockpit) --------
# Shell is added last because it depends on headers from the other three
# modules. The shell CMakeLists handles Qt6 discovery internally and
# degrades gracefully when Qt6 is not available.

add_subdirectory(src/shell/native)

# ---- Tests --------------------------------------------------------------

if(AURA_BUILD_TESTS)
    enable_testing()

    # -- Platform tests ---------------------------------------------------
    add_executable(platform_native_tests
        tests/test_platform/native/test_platform_native.cpp
    )
    target_include_directories(platform_native_tests PRIVATE
        src/runtime/native/include
    )
    target_link_libraries(platform_native_tests PRIVATE aura_platform)
    target_compile_features(platform_native_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(platform_native_tests PRIVATE /W4 /EHsc)
    endif()
    add_test(NAME platform_native_tests COMMAND platform_native_tests)

    # CLI malformed-arg rejection tests (the 'aura' CLI is built by the
    # platform module).
    add_test(
        NAME platform_cli_rejects_malformed_retention
        COMMAND $<TARGET_FILE:aura> --retention-seconds 10foo --no-persist
    )
    set_tests_properties(platform_cli_rejects_malformed_retention
        PROPERTIES WILL_FAIL TRUE
    )
    add_test(
        NAME platform_cli_rejects_malformed_interval
        COMMAND $<TARGET_FILE:aura> --watch --count 1 --interval 1x --no-persist
    )
    set_tests_properties(platform_cli_rejects_malformed_interval
        PROPERTIES WILL_FAIL TRUE
    )

    # -- Telemetry tests --------------------------------------------------
    add_executable(aura_telemetry_native_tests
        tests/test_telemetry/native/test_telemetry_native.cpp
    )
    target_include_directories(aura_telemetry_native_tests PRIVATE
        src/telemetry/native/include
    )
    target_link_libraries(aura_telemetry_native_tests PRIVATE aura_telemetry_native)
    add_test(NAME aura_telemetry_native_tests COMMAND aura_telemetry_native_tests)

    # -- Render tests -----------------------------------------------------
    add_executable(render_native_tests
        tests/test_render/render_native_tests.cpp
    )
    target_include_directories(render_native_tests PRIVATE
        src/render/native/include
    )
    target_link_libraries(render_native_tests PRIVATE aura_render_native)
    add_test(NAME render_native_tests COMMAND render_native_tests)

    # -- Shell tests ------------------------------------------------------
    add_subdirectory(tests/test_shell)
endif()

# ---- Install ------------------------------------------------------------

install(TARGETS aura aura_platform aura_telemetry_native aura_render_native
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
