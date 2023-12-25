# -*- cmake -*-
if (LLINVENTORY_CMAKE_INCLUDED)
  return()
endif (LLINVENTORY_CMAKE_INCLUDED)
set (LLINVENTORY_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/llinventory)

set(LLINVENTORY_LIBRARIES llinventory)
