# -*- cmake -*-

project(llappearance)

include(00-Common)
include(LLCommon)
include(LLCharacter)
include(LLFilesystem)
include(LLImage)
include(LLInventory)
include(LLMath)
include(LLMessage)
include(LLPrimitive)
include(LLRender)
include(LLWindow)
include(LLXML)

set(llappearance_SOURCE_FILES
    llavatarappearance.cpp
    llavatarjoint.cpp
    llavatarjointmesh.cpp
    lldriverparam.cpp
    lllocaltextureobject.cpp
    llpolyskeletaldistortion.cpp
    llpolymesh.cpp
    llpolymorph.cpp
    lltexglobalcolor.cpp
    lltexlayer.cpp
    lltexlayerparams.cpp
    lltexturemanagerbridge.cpp
    llwearable.cpp
    llwearabledata.cpp
    llwearabletype.cpp
    llviewervisualparam.cpp
    llavatarappearancedefines.cpp
    )
    
set(llappearance_HEADER_FILES
    CMakeLists.txt

    llavatarappearance.h
    llavatarjoint.h
    llavatarjointmesh.h
    lldriverparam.h
    lllocaltextureobject.h
    llpolyskeletaldistortion.h
    llpolymesh.h
    llpolymorph.h
    lltexglobalcolor.h
    lltexlayer.h
    lltexlayerparams.h
    lltexturemanagerbridge.h
    llwearable.h
    llwearabledata.h
    llwearabletype.h
    llviewervisualparam.h
    llavatarappearancedefines.h
    )

set_source_files_properties(${llappearance_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llappearance_SOURCE_FILES ${llappearance_HEADER_FILES})

add_library (llappearance ${llappearance_SOURCE_FILES})
add_dependencies(llappearance prepare)

target_link_libraries(llappearance
    ${LLCHARACTER_LIBRARIES}
    ${LLINVENTORY_LIBRARIES}
    ${LLIMAGE_LIBRARIES}
    ${LLRENDER_LIBRARIES}
    ${LLFILESYSTEM_LIBRARIES}
    ${LLXML_LIBRARIES}
    ${LLMATH_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llappearance PRIVATE <linden_common.h> <lljoint.h> <hbfastmap.h>)
  if (USEUNITYBUILD)
    set_target_properties(llappearance PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llappearance COMPILE_DEFINITIONS = ${ALLDEFINES}")
