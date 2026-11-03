# [Codex 2026-10-30] Production web glue, shared by local, CI and browser-test builds.
# Keep the desktop build-ID generator as the single source of commit/date identity.
include(${WONDERLAB_DIR}/cmake/WonderlabBuild.cmake)
get_filename_component(WLA_PLUGIN_DIR "${PLUGIN_SRC_DIR}" DIRECTORY)
set(WLA_WEB_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(wla_web_build_id ALL
    COMMAND ${CMAKE_COMMAND} -DOUT=${WLA_WEB_GENERATED}/build_id.h -DSRC=${WLA_PLUGIN_DIR}
            -P ${WLA_PLUGIN_DIR}/cmake/build_id.cmake
    BYPRODUCTS ${WLA_WEB_GENERATED}/build_id.h
    VERBATIM)
foreach(web_target ${PLUGIN_NAME}Web ${PLUGIN_NAME}WebFallback)
    if(TARGET ${web_target})
        target_sources(${web_target} PRIVATE "${WLA_PLUGIN_DIR}/web/browser-ui.cpp")
        add_dependencies(${web_target} wla_web_build_id)
        target_include_directories(${web_target} PRIVATE "${WLA_WEB_GENERATED}")
    endif()
endforeach()
