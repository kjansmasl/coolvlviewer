# -*- cmake -*-

project(openjpeg)

include(00-Common)

# OPENJPEG version number
set(OPENJPEG_VERSION_MAJOR 1)
set(OPENJPEG_VERSION_MINOR 4)
set(OPENJPEG_VERSION_BUILD 0)
set(OPENJPEG_VERSION
	"${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")

set(openjpeg_SOURCE_FILES
	bio.c
	cio.c
	dwt.c
	event.c
	image.c
	j2k.c
	j2k_lib.c
	jp2.c
	jpt.c
	mct.c
	mqc.c
	openjpeg.c
	pi.c
	raw.c
	t1.c
	t2.c
	tcd.c
	tgt.c
)

set(openjpeg_HEADER_FILES
	bio.h
	cio.h
	dwt.h
	event.h 
	fix.h
	image.h
	int.h
	j2k.h
	j2k_lib.h
	jp2.h
	jpt.h
	mct.h
	mqc.h
	openjpeg.h
	opj_includes.h
	opj_malloc.h
	pi.h
	raw.h
	t1.h
	t1_luts.h
	t2.h
	tcd.h
	tgt.h
)

set_source_files_properties(${openjpeg_HEADER_FILES}
							PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND openjpeg_SOURCE_FILES ${openjpeg_HEADER_FILES})

add_library(openjpeg ${openjpeg_SOURCE_FILES})

if (WINDOWS)
	target_compile_definitions(openjpeg PRIVATE -D_CRT_SECURE_NO_WARNINGS -DOPJ_STATIC)
endif (WINDOWS)

# gcc -flto chokes on openjpeg header definitions and sees definitions
# mismatches where there are none... So forget about LTO for this library.
if (USELTO)
	set_property(TARGET openjpeg PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif (USELTO)

# For the arm64 architecture, we need to download the sse2neon.h header before
# trying to build OpenJPEG...
if (ARCH STREQUAL "arm64")
	include(ViewerMiscLibs)
	include_directories(${LIBS_PREBUILT_DIR}/include)
	add_dependencies(openjpeg prepare)
endif (ARCH STREQUAL "arm64")

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("libopenjpeg COMPILE_DEFINITIONS = ${ALLDEFINES}")
