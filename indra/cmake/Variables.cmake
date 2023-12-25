# -*- cmake -*-
if (VARIABLES_CMAKE_INCLUDED)
	return()
endif (VARIABLES_CMAKE_INCLUDED)
set (VARIABLES_CMAKE_INCLUDED TRUE)

# Definitions of variables used throughout the Second Life build process.
#
# Platform variables:
#
#	DARWIN	- macOS
#	LINUX	- Linux
#	WINDOWS	- Windows

# Relative and absolute paths to subtrees.

set(SCRIPTS_DIR ${CMAKE_SOURCE_DIR}/../scripts)

set(LIBS_PREBUILT_DIR ${CMAKE_SOURCE_DIR}/.. CACHE PATH "Location of prebuilt libraries.")

if (${CMAKE_VERSION} VERSION_GREATER 3.1.0)
	# Prevents a warning about arguments as variables or keywords when unquoted
	# happening when comparing "${CMAKE_CXX_COMPILER_ID}" and "MSVC" below...
	cmake_policy(SET CMP0054 NEW)
endif (${CMAKE_VERSION} VERSION_GREATER 3.1.0)

# Only 64 bits builds are now supported, for all platforms.
# *TODO: support for cross-compilation on a different build architecture ?
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64" OR ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
	set(ARCH x86_64)
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64*" OR ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "arm64*")
	set(ARCH arm64)
else ()
	message(FATAL_ERROR "Unsupported architecture ${CMAKE_HOST_SYSTEM_PROCESSOR}, sorry !")
endif ()
message(STATUS "Architecture to build for: ${ARCH}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(DARWIN 1)

	if (${CMAKE_VERSION} VERSION_LESS 3.12.0)
		message(FATAL_ERROR "Minimum cmake version required to build the viewer with Xcode is 3.12.0")
	endif (${CMAKE_VERSION} VERSION_LESS 3.12.0)

	execute_process(
		COMMAND sh -c "xcodebuild -version | grep Xcode	| cut -d ' ' -f2 | cut -d'.' -f1-2"
		OUTPUT_VARIABLE XCODE_VERSION
	)
	string(REGEX REPLACE "(\r?\n)+$" "" XCODE_VERSION "${XCODE_VERSION}")

	if (XCODE_VERSION LESS 11.0)
		message(FATAL_ERROR "Minimum Xcode version required to build the viewer is 11.0")
	endif (XCODE_VERSION LESS 11.0)

	# Use the new Xcode 12 build system if possible
	if (NOT ${CMAKE_VERSION} VERSION_LESS 3.19.0 AND NOT XCODE_VERSION LESS 12.0)
		set(CMAKE_XCODE_BUILD_SYSTEM 12)
	endif ()

	set (GCC_VERSION 0)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
		execute_process(
			COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version"
			COMMAND head -1
			COMMAND sed "s/^[^0-9]*//"
			OUTPUT_VARIABLE CLANG_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		# Let's actually get a numerical version of Clang's version
		STRING(REGEX REPLACE "([0-9]+)\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CLANG_VERSION ${CLANG_VERSION})
		message(STATUS "Clang version (dots removed): ${CLANG_VERSION}")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
		# GCC (or llvm-gcc) is not supported any more for macOS and while
		# ICC might actually work for compiling the viewer, it would require
		# to make approriate changes/additions to the build system. If you feel
		# like it, be my guest and provide me with the necessary patch... HB
		message(FATAL_ERROR "Unsupported compiler on this platform, sorry !")
	else ()
		message(FATAL_ERROR "Unknown compiler !")
	endif ()

	set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
	set(CMAKE_XCODE_ATTRIBUTE_GCC_STRICT_ALIASING NO)
	set(CMAKE_XCODE_ATTRIBUTE_GCC_FAST_MATH NO)
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS sse2)
	set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION NO)
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "")
	# We target macOS 10.12 for std::shared_mutex usage by phmap.h. If you
	# really need to compile the viewer for an older target, you may go down to
	# 10.9 here, on the condition to lower the C++ standard to c++14 (i.e. by
	# changing the CMAKE_CXX_FLAGS accordingly in 00-Common.cmake). HB
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12)

	# Support for Unix Makefiles generator
	if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
		execute_process(COMMAND xcodebuild -version -sdk "${CMAKE_OSX_SYSROOT}" Path | head -n 1 OUTPUT_VARIABLE CMAKE_OSX_SYSROOT)
		string(REGEX REPLACE "(\r?\n)+$" "" CMAKE_OSX_SYSROOT "${CMAKE_OSX_SYSROOT}")
	endif (CMAKE_GENERATOR STREQUAL "Unix Makefiles")

	message(STATUS "Xcode version: ${XCODE_VERSION}")
	message(STATUS "OSX sysroot: ${CMAKE_OSX_SYSROOT}")
	message(STATUS "OSX deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
	# NOTE: CMAKE_OSX_ARCHITECTURES is apparently always empty...
	#message(STATUS "OSX target architecture: ${CMAKE_OSX_ARCHITECTURES}")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LINUX ON BOOl FORCE)

	set (GCC_VERSION 0)
	set (CLANG_VERSION 0)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		execute_process(
			COMMAND sh -c "${CMAKE_CXX_COMPILER} -dumpversion"
			OUTPUT_VARIABLE CLANG_VERSION
		)
		# Let's actually get a numerical version of Clang's version
		STRING(REGEX REPLACE "([0-9]+)\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CLANG_VERSION ${CLANG_VERSION})
		message(STATUS "Clang version (dots removed): ${CLANG_VERSION}")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		execute_process(
			COMMAND sh -c "${CMAKE_CXX_COMPILER} -dumpversion"
			OUTPUT_VARIABLE GCC_VERSION
		)
		# Let's actually get a numerical version of GCC's version
		STRING(REGEX REPLACE "([0-9]+)\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" GCC_VERSION ${GCC_VERSION})

		# When compiled with --with-gcc-major-version-only newer gcc versions
		# only report the major number with -dumpversion, and -dumpfullversion
		# (which is not understood by older gcc versions) must be used instead
		# to get the true version !!!  The guy who coded this (instead of
		# simply adding a -dumpmajorversion) should face death penalty for
		# utter (and lethal, natural-selection wise) stupidity !
		if (${GCC_VERSION} LESS 100)
			execute_process(
				COMMAND sh -c "${CMAKE_CXX_COMPILER} -dumpfullversion"
				OUTPUT_VARIABLE GCC_VERSION
			)
			STRING(REGEX REPLACE "([0-9]+)\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" GCC_VERSION ${GCC_VERSION})	
		endif (${GCC_VERSION} LESS 100)
		message(STATUS "GCC version (dots removed): ${GCC_VERSION}")
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
		# ICC might actually work for compiling the viewer, but would require
		# to make approriate changes/additions to 00-Common.cmake. If you feel
		# like it, be my guest and provide me with the necessary patch... HB
		message(FATAL_ERROR "Unsupported compiler on this platform, sorry !")
	else ()
		message(FATAL_ERROR "Unknown compiler !")
	endif ()
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(USING_CLANG OFF)
	if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
		set(USING_CLANG ON)
	elseif (MSVC_VERSION EQUAL 1937)
		message(FATAL_ERROR "The MSVC1937 compiler is utterly broken (e.g. the builds crash when ran under Wine), please roll back to MSVC1936 or update to a fixed version !")
	elseif (MSVC_VERSION LESS 1930 OR MSVC_VERSION GREATER 1939)
		message(FATAL_ERROR "You need VS2022 to build this viewer !")
	else ()
		message(STATUS "MSVC version: ${MSVC_VERSION}")
	endif ()

	set(WINDOWS ON BOOL FORCE)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

if (DARWIN)
	set(PREBUILT_TYPE darwin64)
elseif (LINUX)
	if (ARCH STREQUAL "arm64")
		set(PREBUILT_TYPE linux64-arm)
	else ()
		set(PREBUILT_TYPE linux64)
	endif ()
elseif (WINDOWS)
	set(PREBUILT_TYPE windows64)
endif ()

set(VIEWER_BRANDING_ID "cool_vl_viewer" CACHE STRING "Viewer branding id (currently cool_vl_viewer)")
set(VIEWER_BRANDING_NAME "Cool VL Viewer")
set(VIEWER_BRANDING_NAME_CAMELCASE "CoolVLViewer")

set(USESYSTEMLIBS OFF CACHE BOOL "Use system libraries instead of prebuilt libraries whenever possible.")

# NOTE: USEAVX and USEAVX2 are mainly geared towards the MSVC compiler. For gcc
# or clang, you would rather pass -mavx[2] or -march=<cpu-type or native> in
# CMAKE_C_FLAGS and CMAKE_CXX_FLAGS[_RELEASE]. In particular, -march=native
# would automatically enable the adequate SSE2/AVX/AVX2 optimizations for
# compiler-generated maths on the build system.
set(USEAVX OFF CACHE BOOL "Use AVX instead of SSE2 for compiler-generated maths.")
set(USEAVX2 OFF CACHE BOOL "Use AVX2 instead of SSE2 for compiler-generated maths.")

# NOTE: LTO may actually prove detrimental (or simply neutral) to frame rates,
# because the compiler will try too hard to common up code that would otherwise
# get inlined... It appears, however that newer compilers (gcc 11/clang 12) do
# provide better results (frame rates) with LTO, especially llvm/clang v12.
set(USELTO OFF CACHE BOOL "Use link time optimization (Linux only and not supported by all compiler versions).")

# Use unity builds where possible (only available with cmake v3.16.0 or newer;
# this option is simply ignored with older versions). EXPERIMENTAL and largely
# untested: may result in weird bugs in the final binaries... See the comment
# at the end of indra/llplugin/CMakeLists.txt.
set(USEUNITYBUILD OFF CACHE BOOL "Use cmake v3.16.0+ UNITY_BUILD feature for faster builds.")

# Use -fstack-protector option to protect the stack with canaries (causes a
# small loss in speed due to added code and caches usage for each function
# call). Not really needed for an application such as the SL viewer; the risk
# of seeing some injected rogue data triggering an overflow bug and allowing
# arbitrary code execution (and without crashing the viewer, i.e. without the
# user noticing something is wrong) is totally negligible and hardly feasible
# at all for someone not controlling the grid servers.
set(PROTECTSTACK OFF CACHE BOOL "Protect the stack against overflows (for the paranoids).")

# Prevent the use of an executable stack by gcc, if requested. Note: this is
# EXPERIMENTAL and may result in slightly slower code or, at worst, a crashing
# viewer (see the corresponding comment in 00-Common.cmake).
set(NOEXECSTACK OFF CACHE BOOL "Prevent the use of an executable stack by gcc (for the paranoids).")

# OpenMP support, if requested
set(OPENMP OFF CACHE BOOL "Enable OpenMP optimizations.")

# Enable the Tracy profiler support, if requested.
set(TRACY OFF CACHE BOOL "Enable Tracy profiler support.")

# Profiling with gprof, if requested (Linux only).
set(GPROF OFF CACHE BOOL "Enable gprof profiling.")

source_group("CMake Rules" FILES CMakeLists.txt)
