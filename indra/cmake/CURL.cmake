# -*- cmake -*-
if (CURL_CMAKE_INCLUDED)
  return()
endif (CURL_CMAKE_INCLUDED)
set (CURL_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

# Curl depends on OpenSSL...
include(OpenSSL)

if (USE_NEW_LIBCURL)
	# The new lib curl versions depend on nghttp2...
	include(NGHTTP2)
	# When the OpenSSL version present on the system does not match the ones we
	# support (v1.0 or v1.1), OPENSSL_FOUND is set to OFF by OpenSSL.cmake, and
	# our custom OpenSSL is then used, which mandates using our custom curl as
	# well !
	if (USESYSTEMLIBS AND OPENSSL_FOUND)
		set(CURL_FIND_QUIETLY ON)
		set(CURL_FIND_REQUIRED ON)
		include(FindCURL)
	endif (USESYSTEMLIBS AND OPENSSL_FOUND)
endif (USE_NEW_LIBCURL)

if (NOT CURL_INCLUDE_DIRS OR NOT CURL_LIBRARIES)
	if (USE_NEW_LIBCURL)
		use_prebuilt_binary(curl)
	else (USE_NEW_LIBCURL)
		use_prebuilt_binary(curl_old)
	endif (USE_NEW_LIBCURL)

	if (WINDOWS)
		add_definitions(-DCURL_STATICLIB=1)
		set(CURL_LIBRARIES libcurl)
	else (WINDOWS)
		set(CURL_LIBRARIES curl)
	endif (WINDOWS)
	set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (NOT CURL_INCLUDE_DIRS OR NOT CURL_LIBRARIES)

include_directories(SYSTEM ${CURL_INCLUDE_DIRS})
