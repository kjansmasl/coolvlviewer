# -*- cmake -*-
if (LLIMAGE_CMAKE_INCLUDED)
  return()
endif (LLIMAGE_CMAKE_INCLUDED)
set (LLIMAGE_CMAKE_INCLUDED TRUE)

include(JPEG)
include(OpenJPEG)
include(PNG)

include_directories(${CMAKE_SOURCE_DIR}/llimage)

set(LLIMAGE_LIBRARIES llimage)
