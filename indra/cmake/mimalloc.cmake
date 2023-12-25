# -*- cmake -*-
if (MIMALLOC_CMAKE_INCLUDED)
  return()
endif (MIMALLOC_CMAKE_INCLUDED)
set (MIMALLOC_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)

if (NOT USE_MIMALLOC)
  return()
endif ()

include(Prebuilt)
use_prebuilt_binary(mimalloc)

if (WINDOWS)
	set(MIMALLOC_LIBRARY mimalloc-override)
else ()
	set(MIMALLOC_OBJECT ${LIBS_PREBUILT_DIR}/lib/release/mimalloc.o)
endif ()

include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include/mimalloc)
