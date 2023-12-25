# -*- cmake -*-
if (TRACY_CMAKE_INCLUDED)
  return()
endif (TRACY_CMAKE_INCLUDED)
set (TRACY_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)

if (USE_TRACY)
	include(Prebuilt)
	use_prebuilt_binary(tracy)

	set(TRACY_LIBRARY tracy)
	include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include/tracy)
endif (USE_TRACY)
