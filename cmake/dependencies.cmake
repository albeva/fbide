# External dependencies.
#
# Config/INI parsing is handled by wxFileConfig — no dependency needed.
# nlohmann/json parses the GitHub Releases API response in the update check.

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)
