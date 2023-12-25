# -*- cmake -*-
if (BUILDOPTIONS_CMAKE_INCLUDED)
	return()
endif (BUILDOPTIONS_CMAKE_INCLUDED)
set (BUILDOPTIONS_CMAKE_INCLUDED TRUE)

include(Variables)

###############################################################################
# Build options.
###############################################################################

# If you want to enable or disable jemalloc viewer builds, this is the place.
# Set ON or OFF as desired.
# NOTE: jemalloc cannot currently replace macOS's neither the Visual C++ run-
# time malloc()... So, this setting is only relevant to Linux builds.
set(USE_JEMALLOC ON)

# HIGHLY EXPERIMENTAL: If you want to enable or disable mimalloc overriding in
# viewer builds, this is the place. Set ON or OFF as desired. NOTE: for Linux
# builds, USE_JEMALLOC above takes precedence when set to ON.
# BEWARE:
# - In spite of the authors' claims about its supposed performances, mimalloc
#   causes a significant increase in memory usage by the viewer (up to +40% in
#   low memory usage conditions, such as in a skybox), while not bringing any
#   speed benefit when compared with jemalloc under Linux or the standard
#   Windows allocator (my tests so far show that mimalloc is in fact slightly
#   slower, by 1% or so on the measured frame rates).
# - macOS builds using mimalloc are totally untested.
set(USE_MIMALLOC OFF)

# Change to ON to disable parallel-hashmap usage (a header-only rewrite of the
# Abseil hash maps) and instead use the (slower) equivalent boost containers.
# In the (unlikely) event you would get a weird crash where you would suspect
# an issue with a phmap container, this is the thing to do.
set(NO_PHMAP OFF)

# Set to OFF if you do not want to build with FMOD support.
set(BUILD_WITH_FMOD ON)

# Change to ON to build with libcurl v7.64.1 (the last pipelining-compatible
# version, once patched with a one-liner) and OpenSSL v1.1.1. Alas, this curl
# version (and in fact all versions after v7.4x) got a buggy pipelining
# implementation causing "rainbow textures" (especially seen in OpenSim grids,
# but sometimes too in SL, depending on the configuration of the CDN server you
# are hitting).
# The good old libcurl v7.47.0 (sadly using the deprecated OpenSSL v1.0.2) is
# still, for now, the only safe and reliable choice. :-(
set(USE_NEW_LIBCURL OFF)

# Change to ON to build against an older CEF version; relevant to Windows only,
# to restore the Win7/8.x compatibility of the CEF plugin.
set(USE_OLD_CEF OFF)

# Set to ON to replace fast timers with Tracy. This results in a slightly
# slower (~1%) viewer. Use for development builds only. NOTE: this may be
# enabled at configuration time instead by passing -DTRACY:BOOL=TRUE to cmake.
set(USE_TRACY OFF)
# Set to OFF to disable memory usage profiling when Tracy is enabled. Note that
# only allocations done via the viewer custom allocators are actually logged
# (which represents only part of the total used memory). 
set(TRACY_MEMORY ON)
# Set to ON to keep fast timers along Tracy's when the latter is enabled; the
# resulting binary is then slightly slower, of course.
set(TRACY_WITH_FAST_TIMERS OFF)

# Set to ON to enable Animesh visual params support (Muscadine project).
# Experimental and only supported in the Animesh* sims on the SL Aditi grid.
set(ENABLE_ANIMESH_VISUAL_PARAMS OFF)

# Set to OFF to do away with the netapi32 DLL (Netbios) dependency in Windows
# builds; sadly, this causes the MAC address to change, invalidating all saved
# login passwords...
set(USE_NETBIOS ON)

# Compilation/optimization options: uncomment to enable (may also be passed as
# boolean defines to cmake: see Variables.cmake). Mainly relevant to Windows
# and macOS builds (for Linux, simply use the corresponding options in the
# linux-build.sh script, which will pass the appropriate boolean defines to
# cmake).
#set(USELTO ON)
#set(USEAVX ON)
#set(USEAVX2 ON)
# Please note that the current OpenMP optimizations are totally experimental,
# insufficiently tested, and may result in crashes !
#set(OPENMP ON)
# Set to use cmake v3.16.0+ UNITY_BUILD feature to speed-up the compilation
# (experimental and resulting binaries are untested).
#set(USEUNITYBUILD ON)

###############################################################################
# SELECTION LOGIC: DO NOT EDIT UNLESS YOU KNOW EXACTLY WHAT YOU ARE DOING !
###############################################################################

# We only have Linux support for jemalloc...
if (USE_JEMALLOC AND NOT LINUX)
	set(USE_JEMALLOC OFF)
endif (USE_JEMALLOC AND NOT LINUX)
# jemalloc gets precedence over mimalloc under Linux.
if (USE_JEMALLOC AND LINUX)
	set(USE_MIMALLOC OFF)
endif (USE_JEMALLOC AND LINUX)

# Select audio backend(s):
if (BUILD_WITH_FMOD AND ARCH STREQUAL "arm64")
	# No FMOD Studio package available for arm64...
	set(BUILD_WITH_FMOD OFF)
endif (BUILD_WITH_FMOD AND ARCH STREQUAL "arm64")
if (BUILD_WITH_FMOD)
	if (LINUX)
		set(FMOD ON)
		set(OPENAL ON)
	else (LINUX)
		set(FMOD ON)
		set(OPENAL OFF)
	endif (LINUX)
else (BUILD_WITH_FMOD)
	set(FMOD OFF)
	set(OPENAL ON)
endif (BUILD_WITH_FMOD)

if (USE_OLD_CEF AND NOT WINDOWS)
	# No older CEF available for those.
	set(USE_OLD_CEF OFF)
endif ()

if (DARWIN)
	# OpenMP support is missing from Apple's llvm/clang...
	set(OPENMP OFF)
endif (DARWIN)

if (TRACY)
	set(USE_TRACY ON)
endif (TRACY)

if (LINUX AND ARCH STREQUAL "arm64")
	# Disable jemalloc usage until we got a patched CEF arm64 build that works
	# with it instead of crashing (Spotify's builds currently used are not
	# patched and therefore not compatible with jemalloc)...
	set(USE_JEMALLOC OFF)
	# The corresponding libraries have not yet been compiled for Linux ARM64
	set(USE_MIMALLOC OFF)
	set(USE_TRACY OFF)
endif ()
