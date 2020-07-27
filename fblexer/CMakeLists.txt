#
# Lexer source
#
add_library(fblexer SHARED
    lexer.hpp
    lexer.cpp
    module.hpp
    module.cpp
    sdk/LexerSdk.hpp)

#
# Scintilla bundled with wxWidgets
#
set(WX_SOURCE_DIR D:/Developer/libs/wxWidgets/wxWidgets-3.1.4)
target_include_directories(fblexer PRIVATE
    ${WX_SOURCE_DIR}/src/stc/scintilla/include
    ${WX_SOURCE_DIR}/src/stc/scintilla/lexlib)
target_compile_options(fblexer PRIVATE -DSCI_NAMESPACE)

#
# emit dll to bin/ide folder
#
set_target_properties(fblexer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/ide)
