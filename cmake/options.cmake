# Compiler options configuration
add_library(compiler_options INTERFACE)
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(compiler_options INTERFACE /Zc:__cplusplus /Zc:preprocessor)
endif()

target_compile_definitions(compiler_options INTERFACE
    $<$<CONFIG:Debug>:FBIDE_DEBUG_BUILD>
)
