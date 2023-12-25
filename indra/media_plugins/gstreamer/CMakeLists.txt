# -*- cmake -*-

project(media_plugin_gstreamer)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(MediaPluginBase)
include(FindOpenGL)
include(GStreamerPlugin)

### media_plugin_gstreamer

if (NOT WINDOWS)
  add_definitions(-fPIC)
endif (NOT WINDOWS)

set(media_plugin_gstreamer_SOURCE_FILES
    media_plugin_gstreamer.cpp
    llmediaimplgstreamer_syms.cpp
    )

set(media_plugin_gstreamer_HEADER_FILES
     llmediaimplgstreamer.h
     llmediaimplgstreamer_syms.h
    )

set_source_files_properties(${media_plugin_gstreamer_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND media_plugin_gstreamer_SOURCE_FILES
     ${media_plugin_gstreamer_HEADER_FILES})

add_library(media_plugin_gstreamer
    SHARED
    ${media_plugin_gstreamer_SOURCE_FILES}
)

if (DARWIN)
  # Do not prepend 'lib' to the executable name, and do not embed a full
  # install path (which would wrongly be the sources path) inside the library.
  set_target_properties(media_plugin_gstreamer
    PROPERTIES
    PREFIX ""
    BUILD_WITH_INSTALL_NAME_DIR 1
    BUILD_WITH_INSTALL_RPATH 1
    INSTALL_NAME_DIR "@executable_path"
    LINK_FLAGS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/../base/media_plugin_base.exp"
  )
endif (DARWIN)

if (LINUX)
  set_target_properties(media_plugin_gstreamer
      # Do not prepend 'lib' to the executable name
      PROPERTIES
      PREFIX ""
  )
endif (LINUX)

if (WINDOWS)
  set_target_properties(media_plugin_gstreamer
    PROPERTIES
    LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO /NODEFAULTLIB:LIBCMT"
    )
endif (WINDOWS)

target_link_libraries(media_plugin_gstreamer
  ${LLPLUGIN_LIBRARIES}
  ${MEDIA_PLUGIN_BASE_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${PLUGIN_API_LIBRARIES}
)

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("media_plugins/gstreamer COMPILE_DEFINITIONS = ${ALLDEFINES}")

