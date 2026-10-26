# Compiler options configuration
add_library(compiler_options INTERFACE)
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(compiler_options INTERFACE /Zc:__cplusplus /Zc:preprocessor /EHsc /GR)
endif()
if(MSVC)
    # Read sources and encode narrow literals as UTF-8 instead of the ANSI
    # code page, which turns "变" into '?'.
    target_compile_options(compiler_options INTERFACE /utf-8)
endif()

target_compile_definitions(compiler_options INTERFACE
    $<$<CONFIG:Debug>:FBIDE_DEBUG_BUILD>
    $<$<BOOL:${BUILD_APPIMAGE}>:FBIDE_APPIMAGE_BUILD>
)
