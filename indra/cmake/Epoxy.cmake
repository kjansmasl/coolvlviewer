# -*- cmake -*-
if (EPOXY_CMAKE_INCLUDED)
  return()
endif (EPOXY_CMAKE_INCLUDED)
set (EPOXY_CMAKE_INCLUDED TRUE)

include(Prebuilt)

if (USESYSTEMLIBS)
	include(FindPkgConfig)
	pkg_check_modules(EPOXY epoxy)
endif (USESYSTEMLIBS)

if (NOT EPOXY_FOUND)
	use_prebuilt_binary(libepoxy)
	set(EPOXY_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
	set(EPOXY_LIBRARIES libepoxy.a)
endif (NOT EPOXY_FOUND)

include_directories(SYSTEM ${EPOXY_INCLUDE_DIRS})
