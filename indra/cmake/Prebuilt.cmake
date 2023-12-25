# -*- cmake -*-
if (PREBUILT_CMAKE_INCLUDED)
  return()
endif (PREBUILT_CMAKE_INCLUDED)
set (PREBUILT_CMAKE_INCLUDED TRUE)

macro (use_prebuilt_binary _binary)
  get_property(PREBUILT_PACKAGES TARGET prepare PROPERTY PREBUILT)
  list(FIND PREBUILT_PACKAGES ${_binary} _index)
  if(_index LESS 0)
    set_property(TARGET prepare APPEND PROPERTY PREBUILT ${_binary})
  endif(_index LESS 0)
endmacro (use_prebuilt_binary _binary)
