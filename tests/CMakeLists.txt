# Get GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()
FetchContent_MakeAvailable(googletest)

# Tests
add_executable(tests
    BasicTests.cpp
    ConfigTests.cpp
    KeywordsTests.cpp
    LangTests.cpp
    ThemeTests.cpp
)

# Test data path
target_compile_definitions(tests PRIVATE
    FBIDE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

# Link
target_link_libraries(tests PRIVATE fbide_lib compiler_options gtest_main)
target_include_directories(tests SYSTEM PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_precompile_headers(tests REUSE_FROM fbide_lib)

# Discover tests
include(GoogleTest)
gtest_discover_tests(tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
)
