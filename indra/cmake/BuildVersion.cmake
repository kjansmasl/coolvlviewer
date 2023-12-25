# -*- cmake -*-
if (BUILDVERSION_CMAKE_INCLUDED)
  return()
endif (BUILDVERSION_CMAKE_INCLUDED)
set (BUILDVERSION_CMAKE_INCLUDED TRUE)

function (build_version _target)
  # Read version components from the header file.
  file(STRINGS ${CMAKE_SOURCE_DIR}/llcommon/llversion${_target}.h lines
       REGEX " LL_VERSION_")
  foreach(line ${lines})
    string(REGEX REPLACE ".*LL_VERSION_([A-Z]+).*" "\\1" comp "${line}")
    string(REGEX REPLACE ".* = ([0-9]+);.*" "\\1" value "${line}")
    set(v${comp} "${value}")
  endforeach(line)

  # Compose the version.
  set(${_target}_VER "${vMAJOR}.${vMINOR}.${vBRANCH}.${vRELEASE}")
  if (${_target}_VER MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(STATUS "========================================")
    message(STATUS "Version of ${_target} is ${${_target}_VER}")
    message(STATUS "========================================")
  else (${_target}_VER MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Could not determine ${_target} version (${${_target}_VER})")
  endif (${_target}_VER MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")

  # Report version to caller.
  set(${_target}_VER "${${_target}_VER}" PARENT_SCOPE)
endfunction (build_version)
