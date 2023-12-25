# -*- cmake -*-
if (VIEWERMISCLIBS_CMAKE_INCLUDED)
  return()
endif (VIEWERMISCLIBS_CMAKE_INCLUDED)
set (VIEWERMISCLIBS_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

# Vivox binaries only available for x86_64
if (ARCH STREQUAL "x86_64")
	use_prebuilt_binary(vivox)
endif ()

# SSE2 to Neon conversion header is needed for arm64 builds
if (ARCH STREQUAL "arm64")
	use_prebuilt_binary(sse2neon)
endif ()

# *TODO: check for libuuid devel files when USESYSTEMLIBS
if (LINUX AND NOT USESYSTEMLIBS)
	use_prebuilt_binary(libuuid)
endif ()
