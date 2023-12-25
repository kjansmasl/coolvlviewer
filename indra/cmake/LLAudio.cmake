# -*- cmake -*-
if (LLAUDIO_CMAKE_INCLUDED)
  return()
endif (LLAUDIO_CMAKE_INCLUDED)
set (LLAUDIO_CMAKE_INCLUDED TRUE)

include(FMOD)
include(OPENAL)

include_directories(${CMAKE_SOURCE_DIR}/llaudio)

set(LLAUDIO_LIBRARIES llaudio)
