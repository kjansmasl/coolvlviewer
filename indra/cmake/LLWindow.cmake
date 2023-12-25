# -*- cmake -*-
if (LLWINDOW_CMAKE_INCLUDED)
  return()
endif (LLWINDOW_CMAKE_INCLUDED)
set (LLWINDOW_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

if (LINUX)
  if (USESYSTEMLIBS)
    set(SDL2_FIND_QUIETLY ON)
    set(SDL2_FIND_REQUIRED OFF)
    include(FindSDL2)

    # This should be done by FindSDL2.
    mark_as_advanced(
      SDLMAIN_LIBRARY
      SDL2_INCLUDE_DIR
      SDL2_LIBRARY
    )
	set (SDL_LIBRARY ${SDL2_LIBRARY})
	set (SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIR})
  endif (USESYSTEMLIBS)

  if (NOT SDL_INCLUDE_DIR)
    use_prebuilt_binary(libSDL2)
	set (SDL_LIBRARY SDL2)
    set (SDL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
    set (SDL_FOUND "YES")
  endif (NOT SDL_INCLUDE_DIR)

  include_directories(SYSTEM ${SDL2_INCLUDE_DIR})
endif (LINUX)

include_directories(
    # Note: GL/ includes are inside llrender
    ${CMAKE_SOURCE_DIR}/llrender
    ${CMAKE_SOURCE_DIR}/llwindow
    )

set(LLWINDOW_LIBRARIES llwindow)

# llwindowsdl.cpp uses X11 and fontconfig...
if (LINUX)
    list(APPEND LLWINDOW_LIBRARIES X11)

  if (USESYSTEMLIBS)
    include(FindPkgConfig)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    link_directories(${FONTCONFIG_LIBRARY_DIRS})
    list(APPEND LLWINDOW_LIBRARIES ${FONTCONFIG_LIBRARIES})
  else (USESYSTEMLIBS)
    use_prebuilt_binary(fontconfig)
    list(APPEND LLWINDOW_LIBRARIES fontconfig)
    set(FONTCONFIG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/fontconfig)
  endif (USESYSTEMLIBS)
  include_directories(SYSTEM ${FONTCONFIG_INCLUDE_DIRS})
endif (LINUX)

# llwindowwin32.cpp and lldxhardware.cpp use COMDLG32, IMM32, WMI and DXGI.
if (WINDOWS)
    list(APPEND LLWINDOW_LIBRARIES comdlg32 imm32 wbemuuid dxgi)
endif (WINDOWS)
