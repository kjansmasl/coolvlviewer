# -*- cmake -*-
if (LLMATH_CMAKE_INCLUDED)
  return()
endif (LLMATH_CMAKE_INCLUDED)
set (LLMATH_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/llmath)

set(LLMATH_LIBRARIES llmath)
