# -*- cmake -*-

project(llaudio)

include(00-Common)
include(LLAudio)
include(LLAudio)
include(LLCommon)
include(LLFilesystem)
include(LLMath)
include(LLMessage)
include(OggVorbis)

set(llaudio_SOURCE_FILES
    llaudioengine.cpp
    llaudiodecodemgr.cpp
    lllistener.cpp
    llvorbisencode.cpp
    )

set(llaudio_HEADER_FILES
    CMakeLists.txt

    llaudioengine.h
    llaudiodecodemgr.h
    lllistener.h
    llvorbisencode.h
    llwindgen.h
    )

if (FMOD)
    list(APPEND llaudio_SOURCE_FILES
         llaudioengine_fmod.cpp
         lllistener_fmod.cpp
         llstreamingaudio_fmod.cpp
         )

    list(APPEND llaudio_HEADER_FILES
         llaudioengine_fmod.h
         lllistener_fmod.h
         llstreamingaudio_fmod.h
         )
endif ()

if (OPENAL)
  list(APPEND llaudio_SOURCE_FILES
    llaudioengine_openal.cpp
    lllistener_openal.cpp
    )
  
  list(APPEND llaudio_HEADER_FILES
    llaudioengine_openal.h
    lllistener_openal.h
    )
endif (OPENAL)

set_source_files_properties(${llaudio_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llaudio_SOURCE_FILES ${llaudio_HEADER_FILES})

add_library (llaudio ${llaudio_SOURCE_FILES})
add_dependencies(llaudio prepare)
target_link_libraries(
    llaudio
    ${VORBISENC_LIBRARIES}
    ${VORBISFILE_LIBRARIES}
    ${VORBIS_LIBRARIES}
    ${OGG_LIBRARIES}
    ${FMOD_LIBRARY}
    ${OPENAL_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  if (USEUNITYBUILD)
    set_target_properties(llaudio PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  else (USEUNITYBUILD)
    target_precompile_headers(llaudio PRIVATE <linden_common.h>)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llaudio COMPILE_DEFINITIONS = ${ALLDEFINES}")
