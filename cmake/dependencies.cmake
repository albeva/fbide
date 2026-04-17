# toml++
#include(FetchContent)
#FetchContent_Declare(
#    tomlplusplus
#    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
#    GIT_TAG v3.4.0
#)
#FetchContent_MakeAvailable(tomlplusplus)
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG        v4.4.0
)
FetchContent_MakeAvailable(toml11)
