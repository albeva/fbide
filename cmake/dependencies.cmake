# External dependencies.
#
# Config/INI parsing is handled by wxFileConfig — no dependency needed.
# nlohmann/json is used by the AI provider to build request bodies and
# parse Anthropic API responses. maddy renders the markdown in chat
# replies to HTML for the chat panel. md4c parses chat markdown into a
# document model for the custom-painted AI chat view.

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# maddy is header-only. SOURCE_SUBDIR points at a directory that does not
# exist so MakeAvailable only downloads the sources — it does not run
# maddy's own CMakeLists (which would build maddy's test suite).
FetchContent_Declare(
    maddy
    GIT_REPOSITORY https://github.com/progsource/maddy.git
    GIT_TAG 1.5.0
    SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(maddy)

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
