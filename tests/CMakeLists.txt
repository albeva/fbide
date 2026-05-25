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

# Console-mode unit tests (wxAppConsole entry).
add_executable(tests
    unit/main.cpp
    unit/AiContextTests.cpp
    unit/AiTypesTests.cpp
    unit/AnthropicProviderTests.cpp
    unit/ApplyPatchToolTests.cpp
    unit/ReadFileToolTests.cpp
    unit/ToolDispatchLoopTests.cpp
    unit/ToolRegistryTests.cpp
    unit/AutoIndentTests.cpp
    unit/BasicTests.cpp
    unit/MarkdownDocumentTests.cpp
    unit/MockProviderTests.cpp
    unit/OllamaProviderTests.cpp
    unit/MarkdownImageCacheTests.cpp
    unit/MarkdownLayoutTests.cpp
    unit/MarkdownSelectionTests.cpp
    unit/CodeHighlighterTests.cpp
    unit/CompileCommandTests.cpp
    unit/ConfigManagerTests.cpp
    unit/ConfigStrategyTests.cpp
    unit/DocumentIOTests.cpp
    unit/DocumentPathTests.cpp
    unit/EncodingDetectorTests.cpp
    unit/FBSciLexerTests.cpp
    unit/GeminiProviderTests.cpp
    unit/MarkdownTests.cpp
    unit/LineHistoryTests.cpp
    unit/PatchTests.cpp
    unit/ReFormatterTests.cpp
    unit/StyleLexerTests.cpp
    unit/ProviderFactoryTests.cpp
    unit/RunCommandTests.cpp
    unit/SymbolBrowserFilterTests.cpp
    unit/SymbolTableTests.cpp
    unit/TextEncodingTests.cpp
    unit/ThemeTests.cpp
    unit/ValueDiffTests.cpp
    unit/ValueMergeTests.cpp
    unit/VersionTests.cpp
)

target_compile_definitions(tests PRIVATE
    FBIDE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

target_link_libraries(tests PRIVATE fbide_lib compiler_options gtest)
target_include_directories(tests SYSTEM PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/scintilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/include
    ${CMAKE_SOURCE_DIR}/third_party/lexilla/lexlib
    ${nlohmann_json_SOURCE_DIR}/include
)
target_include_directories(tests PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
target_precompile_headers(tests REUSE_FROM fbide_lib)

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
    ui/uimain.cpp
    ui/EditorChangeMarginTests.cpp
    ui/EditorTests.cpp
    ui/EditorPerfTests.cpp
    ui/MarkdownViewTests.cpp
)

# Embed the Win32 application manifest so the GUI test runner gets the
# common-controls v6 / DPI settings — without it wxWidgets warns at startup.
if (WIN32)
    target_sources(ui-tests PRIVATE ui/manifest.rc)
endif ()

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

# Label every ui-tests case `gui` so headless CI can exclude the whole
# executable with `ctest -LE gui` — no fragile per-suite name matching.
gtest_discover_tests(ui-tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "gui"
)
