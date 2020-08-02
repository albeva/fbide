#
# Build scintilla lexlib
#
FILE(GLOB lexlibSRCs lexlib/*)
add_library(lexlib STATIC ${lexlibSRCs})
target_include_directories(lexlib PRIVATE include lexlib)
target_compile_options(lexlib PRIVATE -DSCI_NAMESPACE -DSCI_LEXER)
