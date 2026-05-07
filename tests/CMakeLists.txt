# Get GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()
# Keep GoogleTest's headers and libs out of `cmake --install` — it's a
# test-only dependency, not part of the FBIde release payload.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Tests
add_executable(tests
    main.cpp
    AutoIndentTests.cpp
    BasicTests.cpp
    CompileCommandTests.cpp
    DocumentIOTests.cpp
    EncodingDetectorTests.cpp
    FBSciLexerTests.cpp
    ReFormatterTests.cpp
    StyleLexerTests.cpp
    RunCommandTests.cpp
    SymbolTableTests.cpp
    TextEncodingTests.cpp
    ThemeTests.cpp
    VersionTests.cpp
)

# Test data path
target_compile_definitions(tests PRIVATE
    FBIDE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

# Link
target_link_libraries(tests PRIVATE fbide_lib compiler_options gtest)
target_include_directories(tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/scintilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/lexlib
)
target_include_directories(tests PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
target_precompile_headers(tests REUSE_FROM fbide_lib)

# Discover tests
include(GoogleTest)
gtest_discover_tests(tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
)

# UI tests — separate executable that runs under a real wxApp (with a hidden
# top-level frame) so wx GUI controls (wxStyledTextCtrl, ...) can be
# instantiated. Pure unit tests stay in `tests` (wxAppConsole, faster, no
# GUI dependency).
add_executable(ui-tests
    uimain.cpp
    EditorTests.cpp
)

target_compile_definitions(ui-tests PRIVATE
    FBIDE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    FBIDE_TEST_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/resources/"
)

target_link_libraries(ui-tests PRIVATE fbide_lib compiler_options gtest)
target_include_directories(ui-tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/scintilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/lexlib
)
target_include_directories(ui-tests PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
target_precompile_headers(ui-tests REUSE_FROM fbide_lib)

gtest_discover_tests(ui-tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
)
