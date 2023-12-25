# -*- cmake -*-
#
# Compared to other libraries, compiling this one is a mess.  The
# reason is that we have several source files that have two different
# sets of behaviour, depending on whether they're intended to be part
# of the viewer or the map server.
#
# Unfortunately, the affected code is a rat's nest of #ifdefs, so it's
# easier to play compilation tricks than to actually fix the problem.

project(llwindow)

include(00-Common)
include(LLCommon)
include(LLFilesystem)
include(LLImage)
include(LLMath)
include(LLRender)
include(LLWindow)
include(LLXML)

set(llwindow_SOURCE_FILES
    llcursortypes.cpp
    llfindlocale.cpp
    llkeyboard.cpp
    )

set(llwindows_HEADER_FILES
    CMakeLists.txt
    llcursortypes.h
    llfindlocale.h
    llkeyboard.h
    )

set(viewer_SOURCE_FILES
    llwindow.cpp
    llmousehandler.cpp
    )

set(viewer_HEADER_FILES
    llwindow.h
    llpreeditor.h
    llmousehandler.h
    )

# Libraries on which this library depends, needed for Linux builds
# Sort by high-level to low-level
set(llwindow_LINK_LIBRARIES
    ${SDL_LIBRARY}
    )

if (DARWIN)
  list(APPEND llwindow_SOURCE_FILES
    llkeyboardmacosx.cpp
    llwindowmacosx.cpp
    llwindowmacosx-objc.mm
    llopenglview-objc.mm
    )
  list(APPEND llwindow_HEADER_FILES
    llkeyboardmacosx.h
    llwindowmacosx.h
    llwindowmacosx-objc.h
    llopenglview-objc.h
    llappdelegate-objc.h
    )

  # We use a bunch of deprecated system APIs.
  set_source_files_properties(
      llkeyboardmacosx.cpp
      llwindowmacosx.cpp
      PROPERTIES
      COMPILE_FLAGS "-Wno-deprecated-declarations -fpascal-strings"
      )
endif (DARWIN)

if (LINUX)
  list(APPEND viewer_SOURCE_FILES 
       llkeyboardsdl.cpp 
       llwindowsdl.cpp
       )
  list(APPEND viewer_HEADER_FILES
       llkeyboardsdl.h
       llwindowsdl.h
       )
endif (LINUX)

if (WINDOWS)
  list(APPEND llwindow_SOURCE_FILES
       llwindowwin32.cpp
       lldxhardware.cpp
       llkeyboardwin32.cpp
       )
  list(APPEND llwindow_HEADER_FILES
       llwindowwin32.h
       lldxhardware.h
       llkeyboardwin32.h
       )
endif (WINDOWS)

set_source_files_properties(${llwindow_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

if (llwindow_HEADER_FILES)
  list(APPEND llwindow_SOURCE_FILES ${llwindow_HEADER_FILES})
endif (llwindow_HEADER_FILES)
  list(APPEND viewer_SOURCE_FILES ${viewer_HEADER_FILES})

add_library (llwindow
  ${llwindow_SOURCE_FILES}
  ${viewer_SOURCE_FILES}
  )
add_dependencies(llwindow prepare)
target_link_libraries (llwindow ${llwindow_LINK_LIBRARIES})

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  if (USEUNITYBUILD)
    set_target_properties(llwindow PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  else (USEUNITYBUILD)
    target_precompile_headers(llwindow PRIVATE <linden_common.h>)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llwindow COMPILE_DEFINITIONS = ${ALLDEFINES}")
