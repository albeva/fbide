# fbide library
add_subdirectory(lib)

# fbide executable
if (WIN32)
    add_executable(fbide WIN32)
    target_sources(fbide PRIVATE lib/rc/app.rc)
    if (MINGW AND CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(fbide PRIVATE -Wl,--gc-sections -Wl,--strip-all)
    endif ()
elseif (APPLE)
    # macos bundle executable
    add_executable(fbide MACOSX_BUNDLE)
    # attach Info.plist
    set_target_properties(fbide PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/src/lib/rc/Info.plist)
    # copy resources/ide to the Bundle
    file(GLOB_RECURSE IDE_RESOURCES ${PROJECT_SOURCE_DIR}/resources/ide/*)
    foreach (FILE ${IDE_RESOURCES})
        file(RELATIVE_PATH REL_PATH
            ${PROJECT_SOURCE_DIR}/resources/ide
            ${FILE}
        )
        get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)
        set_source_files_properties(${FILE}
            PROPERTIES MACOSX_PACKAGE_LOCATION Resources/ide/${REL_DIR}
        )
    endforeach ()
    target_sources(fbide PRIVATE ${IDE_RESOURCES})
else ()
    add_executable(fbide)
endif ()

target_sources(fbide PRIVATE main.cpp)
target_link_libraries(fbide PRIVATE fbide_lib compiler_options compiler_warnings)
target_precompile_headers(fbide REUSE_FROM fbide_lib)
set_target_properties(fbide PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
