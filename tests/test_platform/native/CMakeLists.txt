add_executable(platform_native_tests test_platform_native.cpp)

target_include_directories(platform_native_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/runtime/native/include
)

target_link_libraries(platform_native_tests PRIVATE aura_platform)
target_compile_features(platform_native_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(platform_native_tests PRIVATE /W4 /EHsc)
else()
    target_compile_options(platform_native_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME platform_native_tests COMMAND platform_native_tests)

add_test(
    NAME platform_cli_rejects_malformed_retention
    COMMAND $<TARGET_FILE:aura> --retention-seconds 10foo --no-persist
)
set_tests_properties(
    platform_cli_rejects_malformed_retention
    PROPERTIES
        WILL_FAIL TRUE
)

add_test(
    NAME platform_cli_rejects_malformed_interval
    COMMAND $<TARGET_FILE:aura> --watch --count 1 --interval 1x --no-persist
)
set_tests_properties(
    platform_cli_rejects_malformed_interval
    PROPERTIES
        WILL_FAIL TRUE
)

add_custom_command(
    TARGET platform_native_tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:aura_platform>
        $<TARGET_FILE_DIR:platform_native_tests>
)
