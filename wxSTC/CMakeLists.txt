#
#
#
FILE(GLOB scintillaSRC
    src/scintilla/include/*.h
    src/scintilla/lexlib/*.cxx
    src/scintilla/lexlib/*.h
    src/scintilla/src/*.cxx
    src/scintilla/src/*.h
    )

FILE(GLOB wxSRC
    src/PlatWX.h
    src/PlatWX.cpp
    src/ScintillaWX.h
    src/ScintillaWX.cpp
    src/stc.cpp)

add_library(wxstc STATIC ${scintillaSRC} ${wxSRC})

target_compile_options(wxstc PRIVATE -DSCI_NAMESPACE -D__WX__ -DSCI_EMPTYCATALOGUE)

target_include_directories(wxstc PRIVATE
    include
    src/scintilla/include
    src/scintilla/lexlib
    src/scintilla/src
    src/)

include(${wxWidgets_USE_FILE})

target_link_libraries(wxstc
    INTERFACE ${wxWidgets_LIBRARIES}
    )
