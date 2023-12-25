# -*- cmake -*-

project(llplugin)

include(00-Common)
include(CURL)
include(LLCommon)
include(LLImage)
include(LLMath)
include(LLMessage)
include(LLRender)
include(LLXML)
include(LLWindow)

set(llplugin_SOURCE_FILES
    llpluginclassmedia.cpp
    llplugininstance.cpp
    llpluginmessage.cpp
    llpluginmessagepipe.cpp
    llpluginprocesschild.cpp
    llpluginprocessparent.cpp
    llpluginsharedmemory.cpp
    )

set(llplugin_HEADER_FILES
    CMakeLists.txt

    llpluginclassmedia.h
    llpluginclassmediaowner.h
    llplugininstance.h
    llpluginmessage.h
    llpluginmessageclasses.h
    llpluginmessagepipe.h
    llpluginprocesschild.h
    llpluginprocessparent.h
    llpluginsharedmemory.h
    )

set_source_files_properties(${llplugin_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

if (NOT WINDOWS)
  add_definitions(-fPIC)
endif (NOT WINDOWS)

list(APPEND llplugin_SOURCE_FILES ${llplugin_HEADER_FILES})

add_library(llplugin ${llplugin_SOURCE_FILES})

add_dependencies(llplugin prepare)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llplugin PRIVATE <linden_common.h>)
  # Disabled since it causes the CEF plugin to crash at startup...
#  if (USEUNITYBUILD)
#    set_target_properties(llplugin PROPERTIES UNITY_BUILD ON)
#    # Exclude this one because of undefined references (resolved at link time)
#    set_property(SOURCE llpluginclassmedia.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
#  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llplugin COMPILE_DEFINITIONS = ${ALLDEFINES}")
