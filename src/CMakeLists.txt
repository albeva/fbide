# fbide library
add_library(fbide_lib)
target_sources(fbide_lib
    PRIVATE
    lib/app/App.cpp
    lib/config/Lang.cpp

    PUBLIC FILE_SET HEADERS FILES
    lib/app/App.hpp
    lib/config/Lang.hpp
    lib/config/LangId.hpp
    lib/utils/Macros.hpp
    lib/utils/Unowned.hpp
)

# target settings
target_link_libraries(fbide_lib
    PRIVATE compiler_options compiler_warnings
    PUBLIC ${wxWidgets_LIBRARIES}
)

# precompile header
target_precompile_headers(fbide_lib PUBLIC pch.hpp)

# fbide executable
add_executable(fbide WIN32 main.cpp)
target_link_libraries(fbide PRIVATE fbide_lib compiler_options compiler_warnings)
target_precompile_headers(fbide REUSE_FROM fbide_lib)
set_target_properties(fbide PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
