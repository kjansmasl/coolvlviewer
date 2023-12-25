# -*- cmake -*-
if (LLCOMMON_CMAKE_INCLUDED)
  return()
endif (LLCOMMON_CMAKE_INCLUDED)
set (LLCOMMON_CMAKE_INCLUDED TRUE)

include(APR)
include(Boost)
include(Tracy)

include_directories(${CMAKE_SOURCE_DIR}/llcommon)

if (LINUX)
  # llcommon uses `clock_gettime' which is provided by librt on linux.
  set(LLCOMMON_LIBRARIES llcommon rt)
else (LINUX)
  set(LLCOMMON_LIBRARIES llcommon)
endif (LINUX)
