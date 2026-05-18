# External dependencies.
#
# Config/INI parsing is handled by wxFileConfig — no dependency needed.
# nlohmann/json is used by the AI provider to build request bodies and
# parse Anthropic API responses. maddy renders the markdown in chat
# replies to HTML for the chat panel.

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
