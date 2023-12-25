# -*- cmake -*-
if (GSTREAMER_CMAKE_INCLUDED)
  return()
endif (GSTREAMER_CMAKE_INCLUDED)
set (GSTREAMER_CMAKE_INCLUDED TRUE)

include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)
  pkg_check_modules(GSTREAMER gstreamer-1.0)
  pkg_check_modules(GSTREAMER_PLUGINS_BASE gstreamer-plugins-base-1.0)
endif (USESYSTEMLIBS)

if (NOT GSTREAMER_FOUND OR NOT GSTREAMER_PLUGINS_BASE_FOUND)
  use_prebuilt_binary(gstreamer)
  # possible libxml should have its own .cmake file instead
  use_prebuilt_binary(libxml2)
  set(GSTREAMER_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include/gstreamer-1.0
      ${LIBS_PREBUILT_DIR}/include/glib-2.0
      ${LIBS_PREBUILT_DIR}/include/libxml2
      )
  if (LINUX)
    # We don't need to explicitly link against gstreamer itself, because
    # LLMediaImplGStreamer probes for the system's copy at runtime.
    set(GSTREAMER_LIBRARIES
        gobject-2.0
        gmodule-2.0
        dl
        gthread-2.0
        rt
        glib-2.0
        )
  elseif (DARWIN)
    # We don't need to explicitly link against gstreamer itself, because
    # LLMediaImplGStreamer probes for the system's copy at runtime.
    set(GSTREAMER_LIBRARIES
        gobject-2.0
        gmodule-2.0
        dl
        gthread-2.0
        glib-2.0
        )
  else (LINUX)
    set(GSTREAMER_LIBRARIES)
  endif (LINUX)
endif (NOT GSTREAMER_FOUND OR NOT GSTREAMER_PLUGINS_BASE_FOUND)

include_directories(SYSTEM ${GSTREAMER_INCLUDE_DIRS})
