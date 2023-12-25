# -*- cmake -*-
if (LLFILESYSTEM_CMAKE_INCLUDED)
  return()
endif (LLFILESYSTEM_CMAKE_INCLUDED)
set (LLFILESYSTEM_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/llfilesystem)

set(LLFILESYSTEM_LIBRARIES llfilesystem)
