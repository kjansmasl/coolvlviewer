# -*- cmake -*-
if (MEDIAPLUGINBASE_CMAKE_INCLUDED)
  return()
endif (MEDIAPLUGINBASE_CMAKE_INCLUDED)
set (MEDIAPLUGINBASE_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/media_plugins/base)

set(MEDIA_PLUGIN_BASE_LIBRARIES media_plugin_base)
