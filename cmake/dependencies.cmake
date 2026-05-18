# External dependencies.
#
# Config/INI parsing is handled by wxFileConfig — no dependency needed.
# nlohmann/json is used by the AI provider to build request bodies and
# parse Anthropic API responses.

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)
