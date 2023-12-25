# -*- cmake -*-
if (LLMESSAGE_CMAKE_INCLUDED)
  return()
endif (LLMESSAGE_CMAKE_INCLUDED)
set (LLMESSAGE_CMAKE_INCLUDED TRUE)

include(Boost)
include(CURL)
include(OpenSSL)
include(XmlRpcEpi)
include(ZLIB)

include_directories(${CMAKE_SOURCE_DIR}/llmessage)

set(LLMESSAGE_LIBRARIES llmessage)
