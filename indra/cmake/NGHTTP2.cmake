# -*- cmake -*-
if (NGHTTP2_CMAKE_INCLUDED)
	return()
endif (NGHTTP2_CMAKE_INCLUDED)
set (NGHTTP2_CMAKE_INCLUDED TRUE)

if (NOT CURL_CMAKE_INCLUDED)
	message(FATAL_ERROR "NGHTTP2.cmake must not be included manually: use include(CURL) instead !")
endif (NOT CURL_CMAKE_INCLUDED)

# Note: OPENSSL_FOUND can be used here because this file is only included from
# CURL.cmake and the latter got include(OpenSSL). When the OpenSSL version
# present on the system does not match the ones we support (v1.0 or v1.1),
# OPENSSL_FOUND is set to OFF by OpenSSL.cmake, and our custom OpenSSL is then
# used, which mandates using our custom nghttp2 as well !
if (USESYSTEMLIBS AND OPENSSL_FOUND)
	set(NGHTTP2_FIND_QUIETLY ON)
	set(NGHTTP2_FIND_REQUIRED ON)
	include(FindNGHTTP2)
else (USESYSTEMLIBS AND OPENSSL_FOUND)
	use_prebuilt_binary(nghttp2)
	if (WINDOWS)
		set(NGHTTP2_LIBRARIES nghttp2.lib)
	elseif (DARWIN)
		set(NGHTTP2_LIBRARIES libnghttp2.dylib)
	elseif (LINUX)
	set(NGHTTP2_LIBRARIES libnghttp2.a)
	endif (WINDOWS)
	set(NGHTTP2_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/nghttp2)
endif (USESYSTEMLIBS AND OPENSSL_FOUND)

include_directories(SYSTEM ${NGHTTP2_INCLUDE_DIR})
