# -*- cmake -*-

project(llxml)

include(00-Common)
include(LLCommon)
include(LLMath)
include(LLXML)

set(llxml_SOURCE_FILES
    llcontrol.cpp
    llxmlnode.cpp
    llxmlparser.cpp
    llxmltree.cpp
    )

set(llxml_HEADER_FILES
    CMakeLists.txt

    llcontrol.h
    llxmlnode.h
    llxmlparser.h
    llxmltree.h
    )

set_source_files_properties(${llxml_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llxml_SOURCE_FILES ${llxml_HEADER_FILES})

add_library (llxml ${llxml_SOURCE_FILES})
add_dependencies(llxml prepare)
target_link_libraries(
    llxml
    llmath
    ${EXPAT_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  if (USEUNITYBUILD)
    set_target_properties(llxml PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  else (USEUNITYBUILD)
    target_precompile_headers(llxml PRIVATE <linden_common.h>)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llxml COMPILE_DEFINITIONS = ${ALLDEFINES}")
