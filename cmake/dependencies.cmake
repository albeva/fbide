# Boost — kept configured and available though FBIde no longer consumes it
# directly: project / node identifiers are now generated in-house (random
# base-62 / counter — see `utils/Identifier`). The lightweight, header-only
# `uuid` library is still pulled via `BOOST_INCLUDE_LIBRARIES` and linked so
# the dependency stays wired and ready for future use without fetching all of
# Boost.
#
# Fetched from the upstream Boost release tarball — not picked up from
# a system install (Homebrew / apt) so build environments stay
# reproducible without per-host setup.
set(BOOST_INCLUDE_LIBRARIES uuid)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Boost)

# Boost's headers don't compile clean under our `-Werror -Wsign-conversion`
# strictness (upstream sign-conversion warnings in uuid_clock /
# string_generator, etc.). Walk Boost's build tree and mark every
# materialised target as SYSTEM so their interface include directories
# are exempt — upstream warnings are their problem. Applied recursively
# so any future library added via `BOOST_INCLUDE_LIBRARIES` inherits
# the treatment without per-library wiring.
FetchContent_GetProperties(Boost BINARY_DIR _boost_binary_dir)
function(_fbide_mark_boost_system_recursive dir)
    get_property(_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target ${_targets})
        # `set_target_properties` can't operate on ALIAS targets; resolve
        # to the aliased real target first.
        get_target_property(_aliased ${_target} ALIASED_TARGET)
        if(_aliased)
            set(_target ${_aliased})
        endif()
        set_target_properties(${_target} PROPERTIES SYSTEM TRUE)
    endforeach()
    get_property(_subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(_subdir ${_subdirs})
        _fbide_mark_boost_system_recursive(${_subdir})
    endforeach()
endfunction()
_fbide_mark_boost_system_recursive(${_boost_binary_dir})

# nlohmann/json parses the GitHub Releases API response in the update check.
# (Config/INI parsing is handled by wxFileConfig — no dependency needed.)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# md4c is a small C library. SOURCE_SUBDIR skips its own CMakeLists (which
# would also build the md2html executable). Only the core parser
# (src/md4c.c) is needed — not the bundled HTML renderer. It is compiled
# into a dedicated static library so the project's -Werror / /WX warning
# level is not applied to third-party C code.
FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG release-0.5.2
    SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(md4c)

add_library(md4c STATIC "${md4c_SOURCE_DIR}/src/md4c.c")
target_include_directories(md4c SYSTEM PUBLIC "${md4c_SOURCE_DIR}/src")
