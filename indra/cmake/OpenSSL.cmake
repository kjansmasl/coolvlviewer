# -*- cmake -*-
if (OPENSSL_CMAKE_INCLUDED)
	return()
endif (OPENSSL_CMAKE_INCLUDED)
set (OPENSSL_CMAKE_INCLUDED TRUE)

if (NOT CURL_CMAKE_INCLUDED)
	message(FATAL_ERROR "OpenSSL.cmake must not be included manually: use include(CURL) instead !")
endif (NOT CURL_CMAKE_INCLUDED)

set(OpenSSL_FIND_QUIETLY ON)
set(OpenSSL_FIND_REQUIRED OFF)

# Disable system libraries for OpenSSL, because we want to use our own patched
# curl versions (with HTTP/1.1 support), at least until HTTP/2 is enabled in SL
# servers... OPENSSL_FOUND will be OFF as a result of disabling the three
# following lines, and consequently our pre-built curl (and nghttp2, if needed)
# will be loaded by CURL.cmake (and NGHTTP2.cmake).
#if (USESYSTEMLIBS AND USE_NEW_LIBCURL)
#	include(FindOpenSSL)
#endif (USESYSTEMLIBS AND USE_NEW_LIBCURL)

if (NOT OPENSSL_FOUND OR OPENSSL_VERSION VERSION_LESS "1.0" OR NOT (OPENSSL_VERSION VERSION_LESS "1.2"))
	set(OPENSSL_FOUND OFF)
	if (USE_NEW_LIBCURL)
		use_prebuilt_binary(openssl)
	else (USE_NEW_LIBCURL)
		use_prebuilt_binary(openssl_old)
	endif (USE_NEW_LIBCURL)
	if (WINDOWS)
		if (USE_NEW_LIBCURL)
			set(OPENSSL_LIBRARIES libssl libcrypto)
		else (USE_NEW_LIBCURL)
			set(OPENSSL_LIBRARIES ssleay32 libeay32)
		endif (USE_NEW_LIBCURL)
	else (WINDOWS)
		set(OPENSSL_LIBRARIES ssl)
	endif (WINDOWS)
	set(OPENSSL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif ()

if (LINUX)
	set(CRYPTO_LIBRARIES crypto dl)
elseif (DARWIN)
	set(CRYPTO_LIBRARIES crypto)
endif (LINUX)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIRS})
