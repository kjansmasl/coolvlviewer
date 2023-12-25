# -*- cmake -*-
if (LLRENDER_CMAKE_INCLUDED)
  return()
endif (LLRENDER_CMAKE_INCLUDED)
set (LLRENDER_CMAKE_INCLUDED TRUE)

include(Epoxy)
include(FreeType)

include_directories(${CMAKE_SOURCE_DIR}/llrender)

set(LLRENDER_LIBRARIES llrender ${FREETYPE2_LIBRARIES} ${EPOXY_LIBRARIES})
