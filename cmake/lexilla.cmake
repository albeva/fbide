# Lexilla lexlib static library
# Required on non-Windows builds where wxWidgets is a shared library
# and doesn't export lexilla's internal lexlib symbols.

if(NOT WXWIN)
    set(LEXILLA_DIR "${PROJECT_SOURCE_DIR}/third_party/lexilla")

    add_library(lexilla_lexlib STATIC
        ${LEXILLA_DIR}/lexlib/DefaultLexer.cxx
        ${LEXILLA_DIR}/lexlib/LexAccessor.cxx
        ${LEXILLA_DIR}/lexlib/PropSetSimple.cxx
        ${LEXILLA_DIR}/lexlib/StyleContext.cxx
        ${LEXILLA_DIR}/lexlib/WordList.cxx
    )

    target_include_directories(lexilla_lexlib SYSTEM PUBLIC
        ${LEXILLA_DIR}/include
        ${LEXILLA_DIR}/lexlib
        ${PROJECT_SOURCE_DIR}/third_party/scintilla/include
    )

    target_link_libraries(lexilla_lexlib PRIVATE compiler_options)

    # Suppress warnings in third-party code
    if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(lexilla_lexlib PRIVATE -w)
    endif()
endif()
