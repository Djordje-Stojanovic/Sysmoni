add_executable(platform_native_tests test_platform_native.cpp)

target_include_directories(platform_native_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/runtime/native/include
)

target_link_libraries(platform_native_tests PRIVATE aura_platform)

if(MSVC)
    target_compile_options(platform_native_tests PRIVATE /W4 /EHsc)
else()
    target_compile_options(platform_native_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME platform_native_tests COMMAND platform_native_tests)
