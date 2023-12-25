# -*- cmake -*-
if (HACD_CMAKE_INCLUDED)
  return()
endif (HACD_CMAKE_INCLUDED)
set (HACD_CMAKE_INCLUDED TRUE)

# NOTE: it is extremely unlikely that our custom HACD is installed on a system.
# So we unconditionally use our pre-built library, even when USESYSTEMLIBS is
# ON. HB
include(Prebuilt)
use_prebuilt_binary(hacd)

set(HACD_LIBRARY hacd)
set(LLCONVEXDECOMP_LIBRARY nd_hacdConvexDecomposition)
include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include)
