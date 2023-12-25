# -*- cmake -*-

project(llcharacter)

include(00-Common)
include(LLCommon)
include(LLFilesystem)
include(LLMath)
include(LLPrimitive)
include(LLMessage)
include(LLXML)

set(llcharacter_SOURCE_FILES
    llanimationstates.cpp
    llbvhloader.cpp
    llcharacter.cpp
    lleditingmotion.cpp
    llemote.cpp
    llgesture.cpp
    llhandmotion.cpp
    llheadrotmotion.cpp
    lljoint.cpp
    lljointsolverrp3.cpp
    llkeyframefallmotion.cpp
    llkeyframemotion.cpp
    llkeyframestandmotion.cpp
    llkeyframewalkmotion.cpp
    llik.cpp
    llmotion.cpp
    llmotioncontroller.cpp
    llmultigesture.cpp
    llpose.cpp
    lltargetingmotion.cpp
    llvisualparam.cpp
    )

set(llcharacter_HEADER_FILES
    CMakeLists.txt

    llanimationstates.h
    llbvhloader.h
    llbvhconsts.h
    llcharacter.h
    lleditingmotion.h
    llemote.h
    llgesture.h
    llhandmotion.h
    llheadrotmotion.h
    lljoint.h
    lljointsolverrp3.h
    lljointstate.h
    llkeyframefallmotion.h
    llkeyframemotion.h
    llkeyframestandmotion.h
    llkeyframewalkmotion.h
    llik.h
    llmotion.h
    llmotioncontroller.h
    llmultigesture.h
    llpose.h
    lltargetingmotion.h
    llvisualparam.h
    )

set_source_files_properties(${llcharacter_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llcharacter_SOURCE_FILES ${llcharacter_HEADER_FILES})

add_library (llcharacter ${llcharacter_SOURCE_FILES})
add_dependencies(llcharacter prepare)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llcharacter PRIVATE <linden_common.h> <llfasttimer.h> <hbfastmap.h>)
  if (USEUNITYBUILD)
    set_target_properties(llcharacter PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llcharacter COMPILE_DEFINITIONS = ${ALLDEFINES}")
