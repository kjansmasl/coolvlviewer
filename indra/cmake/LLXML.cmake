# -*- cmake -*-
if (LLXML_CMAKE_INCLUDED)
  return()
endif (LLXML_CMAKE_INCLUDED)
set (LLXML_CMAKE_INCLUDED TRUE)

include(Boost)
include(EXPAT)

include_directories(${CMAKE_SOURCE_DIR}/llxml)

set(LLXML_LIBRARIES llxml)
