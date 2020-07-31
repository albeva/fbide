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
#if (MSVC)
#    set(WX_SOURCE_DIR D:/Developer/libs/wxWidgets/wxWidgets-3.1.4)
#elseif(APPLE)
#    set(WX_SOURCE_DIR /Users/albert/Developer/libs/wx/wxWidgets-3.1.4)
#else()
#    set(WX_SOURCE_DIR /home/albert/Developer/libs/wx/wxWidgets-3.1.4)
#endif()

target_include_directories(fblexer SYSTEM PRIVATE
    ${WX_SOURCE_DIR}/src/stc/scintilla/include
    ${WX_SOURCE_DIR}/src/stc/scintilla/lexlib)

target_compile_options(fblexer PRIVATE -DSCI_NAMESPACE)

target_link_libraries(fblexer PRIVATE project_options project_warnings)

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
