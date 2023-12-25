# -*- cmake -*-

project(media_plugin_base)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(FindOpenGL)

### media_plugin_base

if (NOT WINDOWS)
  add_definitions(-fPIC)
endif (NOT WINDOWS)

set(media_plugin_base_SOURCE_FILES
    media_plugin_base.cpp
)

set(media_plugin_base_HEADER_FILES
    CMakeLists.txt

    media_plugin_base.h
    volume_catcher.h
)

# Select which VolumeCatcher implementation to use
if (LINUX)
  if (PULSEAUDIO_FOUND)
    list(APPEND media_plugin_base_SOURCE_FILES linux_volume_catcher.cpp)
  else (PULSEAUDIO_FOUND)
    list(APPEND media_plugin_base_SOURCE_FILES dummy_volume_catcher.cpp)
  endif (PULSEAUDIO_FOUND)
  list(APPEND media_plugin_base_LINK_LIBRARIES
       ${UI_LIBRARIES}     # for glib
       )
elseif (DARWIN)
  # No QuickTime SDK available for 64 bits Darwin builds...
  list(APPEND media_plugin_base_SOURCE_FILES dummy_volume_catcher.cpp)
elseif (WINDOWS)
  list(APPEND media_plugin_base_SOURCE_FILES windows_volume_catcher.cpp)
endif (LINUX)

add_library(media_plugin_base
    ${media_plugin_base_SOURCE_FILES}
)

if (DARWIN)
  find_library(CORESERVICES_LIBRARY CoreServices)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  target_link_libraries(media_plugin_base
       ${CORESERVICES_LIBRARY}     # for Component Manager calls
       ${AUDIOUNIT_LIBRARY}        # for AudioUnit calls
       )
endif (DARWIN)

add_dependencies(media_plugin_base
  prepare
)

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("media_plugins/base COMPILE_DEFINITIONS = ${ALLDEFINES}")
