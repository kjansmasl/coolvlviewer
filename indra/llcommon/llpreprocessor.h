/** 
 * @file llpreprocessor.h
 * @brief This file should be included in all Linden Lab files and
 * should only contain special preprocessor directives
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LLPREPROCESSOR_H
#define LLPREPROCESSOR_H

// Figure out endianness of platform
#ifdef LL_LINUX
# define __ENABLE_WSTRING
# include <endian.h>
#endif	//	LL_LINUX

#if (defined(LL_WINDOWS) || (defined(LL_LINUX) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(LL_DARWIN) && defined(__LITTLE_ENDIAN__)))
# define LL_BIG_ENDIAN 0
#else
# define LL_BIG_ENDIAN 1
#endif

// Mark-up expressions with branch prediction hints. Do NOT use this with
// reckless abandon: it is an obfuscating micro-optimization outside of inner
// loops or other places where you are OVERWHELMINGLY sure which way an
// expression almost-always evaluates.
#if __GNUC__
# define LL_LIKELY(EXPR) __builtin_expect ((bool)(EXPR), true)
# define LL_UNLIKELY(EXPR) __builtin_expect ((bool)(EXPR), false)
#else
# define LL_LIKELY(EXPR) (EXPR)
# define LL_UNLIKELY(EXPR) (EXPR)
#endif

// Figure out differences between compilers
#if defined(__clang__)
	#ifndef LL_CLANG
		#define LL_CLANG 1
	#endif
	#ifndef CLANG_VERSION
		#define CLANG_VERSION (__clang_major__ * 10000 + \
							   __clang_minor__ * 100 + \
							   __clang_patchlevel__)
	#endif
#elif defined(__GNUC__)
	#ifndef LL_GNUC
		#define LL_GNUC 1
	#endif
	#ifndef GCC_VERSION
		#define GCC_VERSION (__GNUC__ * 10000 + \
							 __GNUC_MINOR__ * 100 + \
							 __GNUC_PATCHLEVEL__)
	#endif
#elif defined(__MSVC_VER__) || defined(_MSC_VER)
	#ifndef LL_MSVC
		#define LL_MSVC 1
	#endif
#endif

// No force-inlining for debug builds (letting the compiler options decide)
#if LL_DEBUG || LL_NO_FORCE_INLINE
# define LL_INLINE inline
#elif LL_GNUC || LL_CLANG
# define LL_INLINE inline __attribute__((always_inline))
#elif LL_MSVC
# define LL_INLINE __forceinline
#else
# define LL_INLINE inline
#endif

#if LL_GNUC || LL_CLANG
# define LL_NO_INLINE __attribute__((noinline))
#elif LL_MSVC
# define LL_NO_INLINE __declspec(noinline)
#else
# define LL_NO_INLINE
#endif

#if !defined(__x86_64__) && !(LL_MSVC && _M_X64) && !SSE2NEON
# error "Only 64 builds are now supported, sorry !"
#endif

#if LL_MSVC
// MSVC does not define __SSE__ neither __SSE2__ or __SSE4_1__ ...
# ifndef __SSE__
#  define __SSE__ 1
# endif
# ifndef __SSE2__
#  define __SSE2__ 1
# endif
// When AVX is here, so should be SSE 4.1...
# if !defined(__SSE4_1__) && defined(__AVX__)
#  define __SSE4_1__ 1
# endif
#endif

// Deal with minor differences between OSes.
#if LL_DARWIN || LL_LINUX
// Different name, same functionality.
# define stricmp strcasecmp
# define strnicmp strncasecmp

// Not sure why this is different, but...
# ifndef MAX_PATH
#  define MAX_PATH PATH_MAX
# endif
#endif

// Deal with Visual Studio problems
#if LL_MSVC
# pragma warning(3	     : 4701)	// "local variable used without being initialized"  Treat this as level 3, not level 4.
# pragma warning(3	     : 4189)	// "local variable initialized but not referenced"  Treat this as level 3, not level 4.
# pragma warning(3      :  4263)	// 'function' : member function does not override any base class virtual member function
# pragma warning(3      :  4264)	// "'virtual_function' : no override available for virtual member function from base 'class'; function is hidden"
# pragma warning(3      :  4266)	// 'function' : no override available for virtual member function from base 'type'; function is hidden
# pragma warning(disable : 4180)	// qualifier applied to function type has no meaning; ignored
# pragma warning(disable : 4312)	// conversion from 'type1' to 'type2' of greater size
# pragma warning(disable : 4503)	// 'decorated name length exceeded, name was truncated'. Does not seem to affect compilation.
# pragma warning(disable : 4800)	// 'BOOL' : forcing value to bool 'true' or 'false' (performance warning)
# pragma warning(disable : 4996)	// warning: deprecated

// Linker optimization with "extern template" generates these warnings
# pragma warning(disable : 4231)	// nonstandard extension used : 'extern' before template explicit instantiation

// level 4 warnings that we need to disable:
# pragma warning(disable : 4100)	// unreferenced formal parameter
# pragma warning(disable : 4127)	// conditional expression is constant (e.g. while(1) )
# pragma warning(disable : 4244)	// possible loss of data on conversions
# pragma warning(disable : 4396)	// the inline specifier cannot be used when a friend declaration refers to a specialization of a function template
# pragma warning(disable : 4512)	// assignment operator could not be generated
# pragma warning(disable : 4706)	// assignment within conditional (even if((x = y)) )

# pragma warning(disable : 4251)	// member needs to have dll-interface to be used by clients of class
# pragma warning(disable : 4275)	// non dll-interface class used as base for dll-interface class

#  pragma warning(disable : 4267)	// 'var': conversion from 'size_t' to 'type', possible loss of data)
#endif	//	LL_MSVC

#endif	//	not LL_LINDEN_PREPROCESSOR_H
