#
# scintilla lexlib
#
add_subdirectory(scintilla)

#
# Lexer source
#
add_library(fblexer SHARED
    lexer.hpp
    lexer.cpp
    module.hpp
    module.cpp
    sdk/LexerSdk.hpp)

target_include_directories(fblexer SYSTEM PRIVATE scintilla/include scintilla/lexlib)
target_compile_options(fblexer PRIVATE -DSCI_NAMESPACE -DSCI_LEXER)
target_link_libraries(fblexer PRIVATE lexlib project_options project_warnings)
set_target_properties(fblexer PROPERTIES OUTPUT_NAME "fblexer$<$<CONFIG:Debug>:d>")

#
# emit dll to bin/ide folder
#
if (MSVC)
    set_target_properties(fblexer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/ide)
else()
    set_target_properties(fblexer PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/ide)
endif()
