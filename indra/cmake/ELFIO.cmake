# -*- cmake -*-
if (ELFIO_CMAKE_INCLUDED)
  return()
endif (ELFIO_CMAKE_INCLUDED)
set (ELFIO_CMAKE_INCLUDED TRUE)

if (NOT LINUX)
  return()
endif (NOT LINUX)

include(Prebuilt)

use_prebuilt_binary(elfio)

include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include)
