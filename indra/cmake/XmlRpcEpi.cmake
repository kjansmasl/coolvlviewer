# -*- cmake -*-
if (XMLRPCEPI_CMAKE_INCLUDED)
  return()
endif (XMLRPCEPI_CMAKE_INCLUDED)
set (XMLRPCEPI_CMAKE_INCLUDED TRUE)

include(Prebuilt)

if (USESYSTEMLIBS)
  set(XMLRPCEPI_FIND_QUIETLY ON)
  set(XMLRPCEPI_FIND_REQUIRED OFF)
  include(FindXmlRpcEpi)
endif (USESYSTEMLIBS)

if (NOT XMLRPCEPI_FOUND)
    use_prebuilt_binary(xmlrpc-epi)
    set(XMLRPCEPI_LIBRARIES xmlrpc-epi)
    set(XMLRPCEPI_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
endif (NOT XMLRPCEPI_FOUND)

include_directories(SYSTEM ${XMLRPCEPI_INCLUDE_DIR})
