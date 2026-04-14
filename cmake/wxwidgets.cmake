# wxWidgets configuration

list(APPEND CMAKE_PREFIX_PATH "${WXWIN}")
set(FBIDE_WX_LIBS core base aui stc net)
find_package(wxWidgets CONFIG REQUIRED COMPONENTS ${FBIDE_WX_LIBS})
function(linkWxWidgets target)
    foreach(comp ${FBIDE_WX_LIBS})
        target_link_libraries(${target} PUBLIC wxWidgets::${comp})
    endforeach()
endfunction()
