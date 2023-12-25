# -*- cmake -*-

project(llimage)

include(00-Common)
include(LLCommon)
include(LLFilesystem)
include(LLImage)
include(LLMath)
include(ZLIB)

set(llimage_SOURCE_FILES
    llimagebmp.cpp
    llimage.cpp
    llimagedecodethread.cpp
    llimagej2c.cpp
    llimagejpeg.cpp
    llimagepng.cpp
    llimagetga.cpp
    llpngwrapper.cpp
    )

set(llimage_HEADER_FILES
    CMakeLists.txt

    llimage.h
    llimagedecodethread.h
    llimagebmp.h
    llimagej2c.h
    llimagejpeg.h
    llimagepng.h
    llimagetga.h
    llpngwrapper.h
    )

set_source_files_properties(${llimage_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llimage_SOURCE_FILES ${llimage_HEADER_FILES})

if (WINDOWS)
   add_definitions(-DOPJ_STATIC)
endif (WINDOWS)

add_library (llimage ${llimage_SOURCE_FILES})
add_dependencies(llimage prepare)
target_link_libraries(
    llimage
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${OPENJPEG_LIBRARIES}
    ${ZLIB_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llimage PRIVATE <linden_common.h>)
  if (USEUNITYBUILD)
    set_target_properties(llimage PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llimage COMPILE_DEFINITIONS = ${ALLDEFINES}")
