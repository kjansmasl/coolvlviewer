# -*- cmake -*-
if (LLPLUGIN_CMAKE_INCLUDED)
  return()
endif (LLPLUGIN_CMAKE_INCLUDED)
set (LLPLUGIN_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)

include_directories(${CMAKE_SOURCE_DIR}/llplugin)

set(LLPLUGIN_LIBRARIES llplugin)

if (WINDOWS)
  if (USE_NETBIOS)
    set(NETWORK_LIBS netapi32 iphlpapi)
  else (USE_NETBIOS)
    set(NETWORK_LIBS iphlpapi)
  endif (USE_NETBIOS)

  set(PLUGIN_API_LIBRARIES
      wsock32
      ws2_32
      psapi
      ${NETWORK_LIBS}
      advapi32
      user32
      # Needed by VS2017 to avoid missing printf/scanf/_iob symbols
      legacy_stdio_definitions
      )
elseif (LINUX)
  set(PLUGIN_API_LIBRARIES X11 pthread)
else (WINDOWS)
  set(PLUGIN_API_LIBRARIES "")
endif (WINDOWS)
