# -*- cmake -*-

project(llfilesystem)

include(00-Common)
include(LLCommon)

set(llfilesystem_SOURCE_FILES
    lldir.cpp
    lldiriterator.cpp
    lldiskcache.cpp
    llfilesystem.cpp
    )

set(llfilesystem_HEADER_FILES
    CMakeLists.txt

    lldir.h
    lldiriterator.h
    lldiskcache.h
    llfilesystem.h
    )

if (DARWIN)
  LIST(APPEND llfilesystem_SOURCE_FILES lldir_mac.cpp)
  LIST(APPEND llfilesystem_HEADER_FILES lldir_mac.h)
  LIST(APPEND llfilesystem_SOURCE_FILES lldir_utils_objc.mm)
  LIST(APPEND llfilesystem_HEADER_FILES lldir_utils_objc.h)
endif (DARWIN)

if (LINUX)
  LIST(APPEND llfilesystem_SOURCE_FILES lldir_linux.cpp)
  LIST(APPEND llfilesystem_HEADER_FILES lldir_linux.h)
endif (LINUX)

if (WINDOWS)
  LIST(APPEND llfilesystem_SOURCE_FILES lldir_win32.cpp)
  LIST(APPEND llfilesystem_HEADER_FILES lldir_win32.h)
endif (WINDOWS)

set_source_files_properties(${llfilesystem_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llfilesystem_SOURCE_FILES ${llfilesystem_HEADER_FILES})

add_library (llfilesystem ${llfilesystem_SOURCE_FILES})
add_dependencies(llfilesystem prepare)

if (DARWIN)
  include(CMakeFindFrameworks)
  find_library(COCOA_LIBRARY Cocoa)
endif (DARWIN)
target_link_libraries(llfilesystem
                      ${COCOA_LIBRARY}
                      ${BOOST_FILESYSTEM_LIBRARY}
)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  if (USEUNITYBUILD)
    set_target_properties(llfilesystem PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  else (USEUNITYBUILD)
    target_precompile_headers(llfilesystem PRIVATE <linden_common.h>)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llfilesystem COMPILE_DEFINITIONS = ${ALLDEFINES}")
