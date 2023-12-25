# -*- cmake -*-
if (JPEG_CMAKE_INCLUDED)
  return()
endif (JPEG_CMAKE_INCLUDED)
set (JPEG_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

if (USESYSTEMLIBS)
  set(JPEG_FIND_QUIETLY ON)
  set(JPEG_FIND_REQUIRED OFF)
  include(FindJPEG)
endif (USESYSTEMLIBS)

if (NOT JPEG_FOUND)
  use_prebuilt_binary(jpeglib)
  set(JPEG_LIBRARIES jpeg)
  set(JPEG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/jpeglib)
endif (NOT JPEG_FOUND)

include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})
