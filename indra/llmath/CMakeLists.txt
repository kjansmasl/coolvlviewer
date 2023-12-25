# -*- cmake -*-

project(llmath)

include(00-Common)
include(LLCommon)

set(llmath_SOURCE_FILES
    llbbox.cpp
    llcamera.cpp
    llcolor3.cpp
    llcolor4.cpp
    llcolor4u.cpp
    llcoordframe.cpp
    llinterp.cpp
    llmatrix3.cpp
    llmatrix3a.cpp
    llmatrix4.cpp
    llmatrix4a.cpp
    llnoise.cpp
    llquaternion.cpp
    llraytrace.cpp
    llvector2.cpp
    llvector3.cpp
    llvector3d.cpp
    llvector4.cpp
    llvector4a.cpp
    llsdutil_math.cpp
    )

set(llmath_HEADER_FILES
    CMakeLists.txt

    llbbox.h
    llbboxlocal.h
    llcamera.h
    llcolor3.h
    llcolor4.h
    llcolor4u.h
    llcoord.h
    llcoordframe.h
    llinterp.h
    llmath.h
    llmatrix3.h
    llmatrix3a.h
    llmatrix3a.inl
    llmatrix4.h
    llnoise.h
    llplane.h
    llquantize.h
    llquaternion.h
    llquaternion2.h
    llquaternion2.inl
    llraytrace.h
    llrect.h
    llsimdtypes.h
    llvector2.h
    llvector3.h
    llvector3d.h
    llvector4.h
    llvector4a.h
    llvector4a.inl
    llvector4logical.h
    llsdutil_math.h
    )

set_source_files_properties(${llmath_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llmath_SOURCE_FILES ${llmath_HEADER_FILES})

add_library (llmath ${llmath_SOURCE_FILES})
add_dependencies(llmath prepare)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llmath PRIVATE <linden_common.h>)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llmath COMPILE_DEFINITIONS = ${ALLDEFINES}")
