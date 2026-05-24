# Generated headers + version metadata.
configure_file("config.hpp.in"   "${CMAKE_BINARY_DIR}/configured_files/include/cmake/config.hpp"    ESCAPE_QUOTES)
configure_file("version.rc.in"   "${CMAKE_BINARY_DIR}/configured_files/include/cmake/version.rc"    ESCAPE_QUOTES)
# Win32 application manifest with arch-aware processorArchitecture.
# Lands next to version.rc so app.rc can reach it via the existing
# include path on the rc target. @ONLY keeps stray ${...} in the XML
# from being consumed by configure_file's variable substitution.
configure_file("app.manifest.in" "${CMAKE_BINARY_DIR}/configured_files/include/cmake/app.manifest" @ONLY)
# macOS bundle Info.plist. @ONLY because Apple's plist XML is full of
# bare `$` (none currently, but defensive against future additions).
# Lands under configured_files/macos/ to match the input layout.
if (APPLE)
    configure_file("Info.plist.in" "${CMAKE_BINARY_DIR}/configured_files/macos/Info.plist" @ONLY)
endif ()

# Plain-text version stamp consumed by the release workflow to verify
# the git tag matches the configured version. One line, no newline,
# contains the full version including any pre-release suffix.
file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${FBIDE_FULL_VERSION}")
