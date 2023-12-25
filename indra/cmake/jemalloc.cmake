# -*- cmake -*-
if (JEMALLOC_CMAKE_INCLUDED)
  return()
endif (JEMALLOC_CMAKE_INCLUDED)
set (JEMALLOC_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)

if (NOT USE_JEMALLOC)
  return()
endif ()

include(Prebuilt)
use_prebuilt_binary(jemalloc)

include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include/jemalloc)
set(JEMALLOC_LIBRARY jemalloc)
