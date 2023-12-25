# -*- cmake -*-
if (APR_CMAKE_INCLUDED)
  return()
endif (APR_CMAKE_INCLUDED)
set (APR_CMAKE_INCLUDED TRUE)

include(Linking)
include(Prebuilt)

# Disable system libs support for now, since our APR library is patched,
# which is not the case on a standard system.
#if (USESYSTEMLIBS)
#	set(APR_FIND_QUIETLY ON)
#	set(APR_FIND_REQUIRED OFF)
#	include(FindAPR)
#endif (USESYSTEMLIBS)

if (NOT APR_FOUND)
	use_prebuilt_binary(apr_suite)
	if (WINDOWS)
		add_definitions(-DAPR_DECLARE_STATIC)
		set(APR_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/apr-1.lib)
	elseif (DARWIN)
		set(APR_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libapr-1.a)
	elseif (LINUX)
		# NOTE: under Linux, we cannot use a static libapr-1 library,
		# because the pthread implementation of the library build system
		# would possibly (in fact likely, since we use old Ubuntu 18.4) be
		# different than the one of the viewer build system and ld would
		# throw missing pthread_* symbols... HB
		set(APR_LIBRARIES apr-1 uuid rt)
	endif ()
	set(APR_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/apr-1)
endif (NOT APR_FOUND)

include_directories(SYSTEM ${APR_INCLUDE_DIR})
