# -*- cmake -*-

include(00-Common)

project(cmake)

set(cmake_SOURCE_FILES
    CMakeLists.txt

    00-BuildOptions.cmake
    00-Common.cmake
    APR.cmake
    Boost.cmake
    BuildVersion.cmake
    CEFPlugin.cmake
    CURL.cmake
    DownloadPrebuilt.cmake.in
    ELFIO.cmake
    Epoxy.cmake
    EXPAT.cmake
    FindAPR.cmake
    FindHunSpell.cmake
    FindNDOF.cmake
    FindNGHTTP2.cmake
    FindSDL2.cmake
    FindXmlRpcEpi.cmake
    FMOD.cmake
    FreeType.cmake
    Glib.cmake
    GLOD.cmake
    GStreamerPlugin.cmake
    HACD.cmake
    Hunspell.cmake
    jemalloc.cmake
    JPEG.cmake
    JsonCpp.cmake
    LLAppearance.cmake
    LLAudio.cmake
    LLCharacter.cmake
    LLCommon.cmake
    LLConvexDecomposition.cmake
    LLFilesystem.cmake
    LLImage.cmake
    LLInventory.cmake
    LLMath.cmake
    LLMessage.cmake
    LLPlugin.cmake
    LLPrimitive.cmake
    LLRender.cmake
    LLUI.cmake
    LLWindow.cmake
    LLXML.cmake
    Linking.cmake
    LUA.cmake
    mimalloc.cmake
    NDOF.cmake
    NGHTTP2.cmake
    OggVorbis.cmake
    OPENAL.cmake
    OpenJPEG.cmake
    OpenSSL.cmake
    PNG.cmake
    Python.cmake
    Prebuilt.cmake
    Tracy.cmake
    Variables.cmake
    Versions.cmake
    XmlRpcEpi.cmake
    xxHash.cmake
    ZLIB.cmake
    )

source_group("Shared Rules" FILES ${cmake_SOURCE_FILES})

set(master_SOURCE_FILES
    ../CMakeLists.txt
    ../../scripts/develop.py
    )

source_group("Master Rules" FILES ${master_SOURCE_FILES})

set_source_files_properties(${cmake_SOURCE_FILES} ${master_SOURCE_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(cmake
            cmake_dummy.cpp
            ${cmake_SOURCE_FILES}
            ${master_SOURCE_FILES}
            )
