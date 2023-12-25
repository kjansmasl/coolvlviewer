/** 
 * @file stdtypes.h
 * @brief Basic type declarations for cross platform compatibility.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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
#ifndef LL_STDTYPES_H
#define LL_STDTYPES_H

#include <cfloat>
#include <climits>
#include <stddef.h>

typedef signed char				S8;
typedef unsigned char			U8;
typedef signed short			S16;
typedef unsigned short			U16;
typedef signed int				S32;
typedef unsigned int			U32;

#if LL_WINDOWS
// Windows wchar_t is 16-bit
typedef U32						llwchar;
#else
typedef wchar_t					llwchar;
#endif

#if LL_WINDOWS
typedef signed __int64			S64;
typedef unsigned __int64		U64;
// Probably should be 'hyper' or similiar
# define S64L(a)				(a)
# define U64L(a)				(a)
#else
typedef long long int			S64;
typedef long long unsigned int	U64;
# define S64L(a)				(a##LL)
# define U64L(a)				(a##ULL)
#endif

typedef float					F32;
typedef double					F64;

typedef U8						KEY;
typedef U32						MASK;
typedef U32             		TPACKETID;

// Use #define instead of consts to avoid conversion headaches
#define S8_MAX					(SCHAR_MAX)
#define U8_MAX					(UCHAR_MAX)
#define S16_MAX					(SHRT_MAX)
#define U16_MAX					(USHRT_MAX)
#define S32_MAX					(INT_MAX)
#define U32_MAX					(UINT_MAX)
#define F32_MAX					(FLT_MAX)
#define F64_MAX					(DBL_MAX)

#define S8_MIN					(SCHAR_MIN)
#define U8_MIN					(0)
#define S16_MIN					(SHRT_MIN)
#define U16_MIN					(0)
#define S32_MIN					(INT_MIN)
#define U32_MIN					(0)
#define F32_MIN					(FLT_MIN)
#define F64_MIN					(DBL_MIN)

#ifndef TRUE
# define TRUE					(1)
#endif

#ifndef FALSE
# define FALSE					(0)
#endif

#ifndef NULL
# define NULL					((void*)0)
#endif

typedef U8						LLPCode;

#define	LL_ARRAY_SIZE( _kArray ) ( sizeof( (_kArray) ) / sizeof( _kArray[0] ) )

// The constants below used to be in the now removed lldefs.h header and have
// been moved here for simplicity. HB

// Often used array indices

constexpr U32 VX = 0;
constexpr U32 VY = 1;
constexpr U32 VZ = 2;
constexpr U32 VW = 3;
constexpr U32 VS = 3;

constexpr U32 VRED   = 0;
constexpr U32 VGREEN = 1;
constexpr U32 VBLUE  = 2;
constexpr U32 VALPHA = 3;

// Map constants

constexpr U32 EAST  = 0;
constexpr U32 NORTH = 1;
constexpr U32 WEST  = 2;
constexpr U32 SOUTH = 3;

constexpr U32 NORTHEAST = 4;
constexpr U32 NORTHWEST = 5;
constexpr U32 SOUTHWEST = 6;
constexpr U32 SOUTHEAST = 7;
constexpr U32 MIDDLEMAP = 8;

// *NOTE: These values may be used as hard-coded numbers in scanf() variants.
// --------------
// DO NOT CHANGE.
// --------------

// Buffer size of maximum path + filename string length
constexpr U32 LL_MAX_PATH			= 1024;

// For strings we send in messages
constexpr U32 STD_STRING_BUF_SIZE	= 255;	// Buffer size
constexpr U32 STD_STRING_STR_LEN	= 254;	// String length, \0 excluded

constexpr U32 MAX_STRING			= STD_STRING_BUF_SIZE;	// Buffer size

#endif
