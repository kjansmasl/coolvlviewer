# -*- cmake -*-
if (FMOD_CMAKE_INCLUDED)
  return()
endif (FMOD_CMAKE_INCLUDED)
set (FMOD_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

if (FMOD)
  use_prebuilt_binary(fmodstudio)

  include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include)

  if (DARWIN OR LINUX)
    set(FMOD_LIBRARY fmod)
  elseif (WINDOWS)
    set(FMOD_LIBRARY fmod_vc)
  endif ()

  message(STATUS "Building with FMOD Studio audio support")
  add_definitions(-DLL_FMOD=1)
endif ()

