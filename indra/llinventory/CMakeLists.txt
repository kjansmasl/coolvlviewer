# -*- cmake -*-

project(llinventory)

include(00-Common)
include(LLCommon)
include(LLMath)
include(LLMessage)
include(LLPrimitive)
include(LLXML)

set(llinventory_SOURCE_FILES
    llcategory.cpp
    lleconomy.cpp
    llfoldertype.cpp
    llinventory.cpp
    llinventorytype.cpp
    lllandmark.cpp
    llnotecard.cpp
    llparcel.cpp
    llpermissions.cpp
    llsaleinfo.cpp
    llscriptpermissions.cpp
    llsettingsbase.cpp
    llsettingsdaycycle.cpp
    llsettingssky.cpp
    llsettingstype.cpp
    llsettingswater.cpp
    lltransactionflags.cpp
    )
    
set(llinventory_HEADER_FILES
    CMakeLists.txt

    llcategory.h
    lleconomy.h
    llfoldertype.h
    llinventory.h
    llinventorytype.h
    lllandmark.h
    llnotecard.h
    llparcel.h
    llparcelflags.h
    llpermissions.h
    llpermissionsflags.h
    llregionhandle.h
    llsaleinfo.h
    llscriptpermissions.h
    llsettingsbase.h
    llsettingsdaycycle.h
    llsettingssky.h
    llsettingstype.h
    llsettingswater.h
    lltransactionflags.h
    lltransactiontypes.h
    lluserrelations.h
    )

set_source_files_properties(${llinventory_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llinventory_SOURCE_FILES ${llinventory_HEADER_FILES})

add_library (llinventory ${llinventory_SOURCE_FILES})
add_dependencies(llinventory prepare)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llinventory PRIVATE <linden_common.h> <hbfastmap.h>)
  if (USEUNITYBUILD)
    set_target_properties(llinventory PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llinventory COMPILE_DEFINITIONS = ${ALLDEFINES}")
