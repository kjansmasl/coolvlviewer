# -*- cmake -*-
if (NDOF_CMAKE_INCLUDED)
  return()
endif (NDOF_CMAKE_INCLUDED)
set (NDOF_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

set(NDOF_FIND_REQUIRED OFF)

if (USESYSTEMLIBS)
  include(FindNDOF)
endif (USESYSTEMLIBS)

if (NOT NDOF_FOUND)
  use_prebuilt_binary(ndofdev)

  if (WINDOWS)
    set(NDOF_LIBRARY libndofdev)
  elseif (DARWIN OR LINUX)
    set(NDOF_LIBRARY ndofdev)
  endif ()

  set(NDOF_INCLUDE_DIR ${ARCH_PREBUILT_DIRS}/include/ndofdev)
  set(NDOF_FOUND "YES")
endif (NOT NDOF_FOUND)

include_directories(SYSTEM ${NDOF_INCLUDE_DIR})
