# -*- cmake -*-
if (GLOD_CMAKE_INCLUDED)
  return()
endif (GLOD_CMAKE_INCLUDED)
set (GLOD_CMAKE_INCLUDED TRUE)

include(Prebuilt)

# NOTE: our GLOD library is a patched one, that works with vertex buffers
# (instead of fixed OpenGL functions) and is compatible with the GL core
# profile. No such library exists in any Linux distro, so we always use our
# library, even when USESYSTEMLIBS is ON. HB
use_prebuilt_binary(glod)

set(GLOD_LIBRARIES glod)

include_directories(SYSTEM ${LIBS_PREBUILT_DIR}/include)
