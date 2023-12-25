# -*- cmake -*-

project(llrender)

include(00-Common)
include(FreeType)
include(LLCommon)
include(LLFilesystem)
include(LLImage)
include(LLMath)
include(LLRender)
include(LLWindow)
include(LLXML)

set(llrender_SOURCE_FILES
    llcubemap.cpp
    llcubemaparray.cpp
    llfontbitmapcache.cpp
    llfontfreetype.cpp
    llfontgl.cpp
    llfontregistry.cpp
    llgl.cpp
    llglslshader.cpp
    llgltexture.cpp
    llimagegl.cpp
    llrender.cpp
    llrendertarget.cpp
    llrenderutils.cpp
    llshadermgr.cpp
    llvertexbuffer.cpp
    )
    
set(llrender_HEADER_FILES
    CMakeLists.txt
    llcubemap.h
    llcubemaparray.h
    llfontgl.h
    llfontfreetype.h
    llfontbitmapcache.h
    llfontregistry.h
    llgl.h
    llglheaders.h
    llglslshader.h
    llgltexture.h
    llimagegl.h
    llrender.h
    llrendertarget.h
    llrenderutils.h
    llshadermgr.h
    llvertexbuffer.h
    )

set_source_files_properties(${llrender_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llrender_SOURCE_FILES ${llrender_HEADER_FILES})

add_library (llrender ${llrender_SOURCE_FILES})
add_dependencies(llrender prepare)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llrender PRIVATE <linden_common.h> <hbfastmap.h>)
  if (USEUNITYBUILD)
    set_target_properties(llrender PROPERTIES UNITY_BUILD ON)
    # Needed to avoid failure by cmake to properly include the glh_extensions.h
    # header, resulting in bogus "undeclared identifier gGLHExts" error...
    set_property(SOURCE llgl.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llrender COMPILE_DEFINITIONS = ${ALLDEFINES}")
