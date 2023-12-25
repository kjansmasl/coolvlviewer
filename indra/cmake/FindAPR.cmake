# -*- cmake -*-

# - Find Apache Portable Runtime
# Find the APR includes and libraries
# This module defines
#  APR_INCLUDE_DIR, where to find apr.h, etc.
#  APR_LIBRARIES, the libraries needed to use APR.
#  APR_FOUND, If false, do not try to use APR.
# also defined, but not for general use are
#  APR_LIBRARY, where to find the APR library.

find_path(APR_INCLUDE_DIR apr.h
/usr/local/include/apr-1
/usr/local/include/apr-1.0
/usr/include/apr-1
/usr/include/apr-1.0
)

set(APR_NAMES ${APR_NAMES} apr-1)
find_library(APR_LIBRARY NAMES ${APR_NAMES}
			 PATHS /usr/lib64 /usr/local/lib64 /usr/lib /usr/local/lib)

if (APR_LIBRARY AND APR_INCLUDE_DIR)
	set(APR_LIBRARIES ${APR_LIBRARY})
	set(APR_FOUND "YES")
else (APR_LIBRARY AND APR_INCLUDE_DIR)
	set(APR_FOUND "NO")
endif (APR_LIBRARY AND APR_INCLUDE_DIR)


if (APR_FOUND)
	if (NOT APR_FIND_QUIETLY)
		message(STATUS "Found APR: ${APR_LIBRARIES}")
	endif (NOT APR_FIND_QUIETLY)
else (APR_FOUND)
	if (APR_FIND_REQUIRED)
		message(FATAL_ERROR "Could not find APR library")
	endif (APR_FIND_REQUIRED)
endif (APR_FOUND)
