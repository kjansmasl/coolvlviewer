# -*- cmake -*-
if (OPENAL_CMAKE_INCLUDED)
  return()
endif (OPENAL_CMAKE_INCLUDED)
set (OPENAL_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

if (OPENAL)
  if (USESYSTEMLIBS)
    include(FindPkgConfig)
    include(FindOpenAL)
    pkg_check_modules(OPENAL_LIB openal)
    pkg_check_modules(FREEALUT_LIB freealut)
  endif (USESYSTEMLIBS)
  if (NOT OPENAL_LIB_FOUND OR NOT FREEALUT_LIB_FOUND)
    use_prebuilt_binary(openal-soft)
    set(OPENAL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
  endif (NOT OPENAL_LIB_FOUND OR NOT FREEALUT_LIB_FOUND)
  set(OPENAL_LIBRARIES 
    openal
    alut
    )
endif (OPENAL)

if (OPENAL)
  message(STATUS "Building with OpenAL audio support")
  add_definitions(-DLL_OPENAL=1)
  include_directories(SYSTEM ${OPENAL_INCLUDE_DIR})
endif (OPENAL)
