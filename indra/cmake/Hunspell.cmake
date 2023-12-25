# -*- cmake -*-
if (HUNSPELL_CMAKE_INCLUDED)
  return()
endif (HUNSPELL_CMAKE_INCLUDED)
set (HUNSPELL_CMAKE_INCLUDED TRUE)

include(Prebuilt)

if (USESYSTEMLIBS)
  set(HUNSPELL_FIND_QUIETLY OFF)
  set(HUNSPELL_FIND_REQUIRED OFF)
  include(FindHunSpell)
endif (USESYSTEMLIBS)

if (NOT HUNSPELL_FOUND)
  use_prebuilt_binary(hunspell)

  set(HUNSPELL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/hunspell)

  if (LINUX)
    set(HUNSPELL_LIBRARY hunspell-1.3)
  elseif (DARWIN)
    set(HUNSPELL_LIBRARY libhunspell-1.3.a)
  else (LINUX)
    set(HUNSPELL_LIBRARY libhunspell)
  endif ()
endif (NOT HUNSPELL_FOUND)

include_directories(SYSTEM ${HUNSPELL_INCLUDE_DIR})
