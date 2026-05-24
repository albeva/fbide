# fbide library
add_subdirectory(lib)

# fbide executable
if (WIN32)
    add_executable(fbide WIN32)
    target_sources(fbide PRIVATE lib/rc/app.rc)

    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        if (MINGW)
            target_link_options(fbide PRIVATE -Wl,--gc-sections -Wl,--strip-all)
        elseif (MSVC)
            set_property(TARGET fbide_lib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
            set_property(TARGET fbide PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif ()
    endif ()
elseif (APPLE)
    # macos bundle executable
    add_executable(fbide MACOSX_BUNDLE)

    # Info.plist. Must be set via MACOSX_BUNDLE_INFO_PLIST — adding the
    # file as a source with MACOSX_PACKAGE_LOCATION leaves CMake's empty
    # default-template plist in place, which crashes NSOpenSavePanelService
    # the moment it tries to key NSUserDefaults by an empty CFBundleIdentifier.
    set(FBIDE_INFO_PLIST ${CMAKE_BINARY_DIR}/configured_files/macos/Info.plist)
    set_target_properties(fbide PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${FBIDE_INFO_PLIST}
    )

    # App icons
    set(FBIDE_MACOS_ICON ${PROJECT_SOURCE_DIR}/src/lib/rc/fbide.icns)
    set_source_files_properties(${FBIDE_MACOS_ICON}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(fbide PRIVATE ${FBIDE_MACOS_ICON})

    # copy resources/pristine to the Bundle
    file(GLOB_RECURSE IDE_RESOURCES ${PROJECT_SOURCE_DIR}/resources/pristine/*)
    foreach (FILE ${IDE_RESOURCES})
        file(RELATIVE_PATH REL_PATH
            ${PROJECT_SOURCE_DIR}/resources/pristine
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
