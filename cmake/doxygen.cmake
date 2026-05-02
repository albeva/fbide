# Doxygen integration. Adds a `docs` target when Doxygen (and optionally
# dot from Graphviz) is present. If Doxygen is missing the target is
# skipped silently so cmake configure still succeeds.

find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found - 'docs' target unavailable")
    return()
endif()

if(NOT DOXYGEN_DOT_FOUND)
    message(STATUS "Graphviz dot not found - dependency graphs will be disabled")
    set(FBIDE_DOXY_HAVE_DOT NO)
else()
    set(FBIDE_DOXY_HAVE_DOT YES)
endif()

set(FBIDE_DOXY_IN  "${CMAKE_SOURCE_DIR}/Doxyfile.in")
set(FBIDE_DOXY_OUT "${CMAKE_BINARY_DIR}/Doxyfile")

configure_file("${FBIDE_DOXY_IN}" "${FBIDE_DOXY_OUT}" @ONLY)

add_custom_target(
    docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${FBIDE_DOXY_OUT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating FBIde documentation with Doxygen"
    VERBATIM
)
