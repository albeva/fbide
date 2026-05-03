# Single source of truth for the FBIde version.
#
# Usage at the top of the top-level CMakeLists.txt:
#
#     include(cmake/version.cmake)
#     fbide_version(0.5.0)            # final release
#     fbide_version(0.5.0 RC 1)       # -> 0.5.0.rc-1
#     fbide_version(0.5.0 Beta 2)     # -> 0.5.0.beta-2
#     fbide_version(0.5.0 Alpha 3)    # -> 0.5.0.alpha-3
#     project(fbide VERSION ${FBIDE_VERSION_NUMERIC} ...)
#
# Sets in the parent scope:
#
#     FBIDE_VERSION_NUMERIC   - "MAJOR.MINOR.PATCH" (feed to project())
#     FBIDE_VERSION_TAG       - lowercase tag string, "" for final
#                               ("alpha" | "beta" | "rc" | "")
#     FBIDE_VERSION_TAG_ENUM  - PascalCase enum case for Version::Tag
#                               ("Alpha" | "Beta" | "ReleaseCandidate" | "None")
#     FBIDE_VERSION_TWEAK     - integer iteration (0 when no tag)
#     FBIDE_FULL_VERSION      - "MAJOR.MINOR.PATCH" or
#                               "MAJOR.MINOR.PATCH.tag-tweak"
#
# Unknown tags fail at configure time with FATAL_ERROR — the C++ enum
# substitution `Version::Tag::${FBIDE_VERSION_TAG_ENUM}` is a second
# safety net that fails at compile time if the function is bypassed.
function(fbide_version numeric)
    set(extra ${ARGN})
    list(LENGTH extra nextra)

    if (nextra EQUAL 0)
        set(tag_lower "")
        set(tag_enum "None")
        set(tweak 0)
        set(full "${numeric}")
    elseif (nextra EQUAL 2)
        list(GET extra 0 tag_in)
        list(GET extra 1 tweak)
        if (tag_in STREQUAL "Alpha")
            set(tag_lower "alpha")
            set(tag_enum "Alpha")
        elseif (tag_in STREQUAL "Beta")
            set(tag_lower "beta")
            set(tag_enum "Beta")
        elseif (tag_in STREQUAL "RC")
            set(tag_lower "rc")
            set(tag_enum "ReleaseCandidate")
        else()
            message(FATAL_ERROR
                "fbide_version: unknown tag '${tag_in}' "
                "(expected one of: Alpha, Beta, RC).")
        endif()
        set(full "${numeric}.${tag_lower}-${tweak}")
    else()
        message(FATAL_ERROR
            "fbide_version: expected '<numeric>' or '<numeric> <Tag> <Tweak>', "
            "got '${numeric} ${extra}'.")
    endif()

    set(FBIDE_VERSION_NUMERIC  "${numeric}"   PARENT_SCOPE)
    set(FBIDE_VERSION_TAG      "${tag_lower}" PARENT_SCOPE)
    set(FBIDE_VERSION_TAG_ENUM "${tag_enum}"  PARENT_SCOPE)
    set(FBIDE_VERSION_TWEAK    "${tweak}"     PARENT_SCOPE)
    set(FBIDE_FULL_VERSION     "${full}"      PARENT_SCOPE)
endfunction()
