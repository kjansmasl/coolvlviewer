# -*- cmake -*-
if (PNG_CMAKE_INCLUDED)
  return()
endif (PNG_CMAKE_INCLUDED)
set (PNG_CMAKE_INCLUDED TRUE)

include(Prebuilt)

set(PNG_FIND_QUIETLY ON)
set(PNG_FIND_REQUIRED OFF)

if (USESYSTEMLIBS)
  include(FindPNG)
endif (USESYSTEMLIBS)

if (NOT PNG_FOUND)
  use_prebuilt_binary(libpng)
  if (WINDOWS)
    set(PNG_LIBRARIES libpng16)
  elseif(DARWIN)
    set(PNG_LIBRARIES png16)
  elseif(LINUX)
    #
    # When we have updated static libraries in competition with older shared
    # libraries and we want the former to win, we need to do some extra work.
    # The *_PRELOAD_ARCHIVES settings are invoked early and will pull in the
    # entire archive to the binary giving it priority in symbol resolution.
    # Beware of cmake moving the archive load itself to another place on the
    # link command line. If that happens, you can try something like
    # -Wl,-lpng16 here to hide the archive. Also be aware that the linker will
    # not tolerate a second whole-archive load of the archive. See viewer's
    # CMakeLists.txt for more information.
    #
    set(PNG_PRELOAD_ARCHIVES -Wl,--whole-archive png16 -Wl,--no-whole-archive)
    set(PNG_LIBRARIES png16)
  endif(WINDOWS)
  set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng16)
endif (NOT PNG_FOUND)

include_directories(SYSTEM ${PNG_INCLUDE_DIRS})
