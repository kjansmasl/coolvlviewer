# -*- cmake -*-
if (FREETYPE_CMAKE_INCLUDED)
  return()
endif (FREETYPE_CMAKE_INCLUDED)
set (FREETYPE_CMAKE_INCLUDED TRUE)

include(Prebuilt)

# NOTE: we never use the system freetype2 library, because it would fail to
# display out custom fonts.
#if (USESYSTEMLIBS)
#  include(FindPkgConfig)
#  pkg_check_modules(FREETYPE2 REQUIRED freetype2)
#else (USESYSTEMLIBS)
  use_prebuilt_binary(freetype)
  set(FREETYPE2_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/freetype ${LIBS_PREBUILT_DIR}/include/freetype2 ${LIBS_PREBUILT_DIR}/include/freetype2/freetype)
  set(FREETYPE2_LIBRARIES freetype)
#endif (USESYSTEMLIBS)

include_directories(SYSTEM ${FREETYPE2_INCLUDE_DIRS})
link_directories(${FREETYPE2_LIBRARY_DIRS})
add_definitions(${FREETYPE2_CFLAGS_OTHERS})
