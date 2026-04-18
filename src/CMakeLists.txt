# fbide library
add_subdirectory(lib)

# fbide executable
add_executable(fbide WIN32 main.cpp)
target_link_libraries(fbide PRIVATE fbide_lib compiler_options compiler_warnings)
target_precompile_headers(fbide REUSE_FROM fbide_lib)
set_target_properties(fbide PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if (WIN32)
    target_sources(fbide PRIVATE lib/rc/app.rc)
    target_include_directories(fbide PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
endif ()

if (MINGW AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(fbide PRIVATE -Wl,--gc-sections -Wl,--strip-all)
endif ()
