# -*- cmake -*-
if (LLCHARACTER_CMAKE_INCLUDED)
  return()
endif (LLCHARACTER_CMAKE_INCLUDED)
set (LLCHARACTER_CMAKE_INCLUDED TRUE)

include_directories(${CMAKE_SOURCE_DIR}/llcharacter)

set(LLCHARACTER_LIBRARIES llcharacter)
