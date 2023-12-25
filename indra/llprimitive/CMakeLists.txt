# -*- cmake -*-

project(llprimitive)

include(00-Common)
include(LLCommon)
include(LLCharacter)
include(LLMath)
include(LLMessage)
include(LLPrimitive)
include(LLXML)
include(ZLIB)

set(llprimitive_SOURCE_FILES
    imageids.cpp	
    lldaeloader.cpp
    llgltfloader.cpp
    llgltfmaterial.cpp
    llmaterial.cpp
    llmaterialid.cpp
    llmaterialtable.cpp
    llmediaentry.cpp
    llmeshoptimizer.cpp
    llmodel.cpp
    llmodelloader.cpp
    llphysshapebuilderutil.cpp
    llprimitive.cpp
    llprimtexturelist.cpp
    lltextureanim.cpp
    lltextureentry.cpp
    lltreeparams.cpp
    llvolume.cpp
    llvolumemessage.cpp
    llvolumemgr.cpp
    llvolumeoctree.cpp
    llxform.cpp
    sound_ids.cpp
    )

set(llprimitive_HEADER_FILES
    CMakeLists.txt
    imageids.h
    lldaeloader.h
    llgltfloader.h
    llgltfmaterial.h
    lllslconstants.h
    llmaterial.h
    llmaterialid.h
    llmaterialtable.h
    llmediaentry.h
    llmeshoptimizer.h
    llmodel.h
    llmodelloader.h
    lloctree.h
    llphysshapebuilderutil.h
    llprimitive.h
    llprimtexturelist.h
    lltextureanim.h
    lltextureentry.h
    lltreeparams.h
    lltree_common.h
    llvolume.h
    llvolumemessage.h
    llvolumemgr.h
    llvolumeoctree.h
    llxform.h
    object_flags.h
    sound_ids.h
    )
        
set_source_files_properties(${llprimitive_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llprimitive_SOURCE_FILES ${llprimitive_HEADER_FILES})

add_library (llprimitive ${llprimitive_SOURCE_FILES})
add_dependencies(llprimitive prepare)

target_link_libraries(llprimitive
	meshoptimizer
    ${LLCOMMON_LIBRARIES}
    ${LLMATH_LIBRARIES}
    ${LLMESSAGE_LIBRARIES}
    ${LLXML_LIBRARIES}
    ${LLCHARACTER_LIBRARIES}
    ${ZLIB_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llprimitive PRIVATE <linden_common.h>)
  if (USEUNITYBUILD)
    set_target_properties(llprimitive PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llprimitive COMPILE_DEFINITIONS = ${ALLDEFINES}")
