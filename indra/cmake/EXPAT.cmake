# -*- cmake -*-
if (EXPAT_CMAKE_INCLUDED)
	return()
endif (EXPAT_CMAKE_INCLUDED)
set (EXPAT_CMAKE_INCLUDED TRUE)

include(Prebuilt)

if (USESYSTEMLIBS)
	set(EXPAT_FIND_QUIETLY OFF)
	set(EXPAT_FIND_REQUIRED OFF)
	include(FindEXPAT)
endif (USESYSTEMLIBS)

if (NOT EXPAT_FOUND)
	use_prebuilt_binary(expat)
	if (DARWIN)
		set(EXPAT_LIBRARIES expat)
	elseif (LINUX)
		# Make sure we will link against our static library
		set(EXPAT_LIBRARIES libexpat.a)
    elseif (WINDOWS)
		set(EXPAT_LIBRARIES libexpatMT)
		# Needed by expat under Windows only. HB
		add_definitions(-DXML_STATIC)
	endif ()
	set(EXPAT_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/expat)
endif (NOT EXPAT_FOUND)

include_directories(SYSTEM ${EXPAT_INCLUDE_DIRS})
