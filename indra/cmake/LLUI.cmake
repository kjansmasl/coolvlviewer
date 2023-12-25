# -*- cmake -*-
if (LLUI_CMAKE_INCLUDED)
  return()
endif (LLUI_CMAKE_INCLUDED)
set (LLUI_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/llui)

set(LLUI_LIBRARIES llui)
