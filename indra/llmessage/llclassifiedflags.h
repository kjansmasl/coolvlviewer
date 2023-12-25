/** 
 * @file llclassifiedflags.h
 * @brief Flags used in the classifieds.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLCLASSIFIEDFLAGS_H
#define LL_LLCLASSIFIEDFLAGS_H

#include "llpreprocessor.h"

typedef U8 ClassifiedFlags;

constexpr U8 CLASSIFIED_FLAG_NONE   	= 1 << 0;
constexpr U8 CLASSIFIED_FLAG_MATURE 	= 1 << 1;
//constexpr U8 CLASSIFIED_FLAG_ENABLED	= 1 << 2;	// see llclassifiedflags.cpp
//constexpr U8 CLASSIFIED_FLAG_HAS_PRICE= 1 << 3;	// deprecated
constexpr U8 CLASSIFIED_FLAG_UPDATE_TIME= 1 << 4;
constexpr U8 CLASSIFIED_FLAG_AUTO_RENEW = 1 << 5;

constexpr U8 CLASSIFIED_QUERY_FILTER_MATURE		= 1 << 1;
//constexpr U8 CLASSIFIED_QUERY_FILTER_ENABLED	= 1 << 2;
//constexpr U8 CLASSIFIED_QUERY_FILTER_PRICE	= 1 << 3;

// These are new with Adult-enabled viewers (1.23 and later)
constexpr U8 CLASSIFIED_QUERY_INC_PG			= 1 << 2;
constexpr U8 CLASSIFIED_QUERY_INC_MATURE		= 1 << 3;
constexpr U8 CLASSIFIED_QUERY_INC_ADULT			= 1 << 6;
constexpr U8 CLASSIFIED_QUERY_INC_NEW_VIEWER	= (CLASSIFIED_QUERY_INC_PG |
												   CLASSIFIED_QUERY_INC_MATURE |
												   CLASSIFIED_QUERY_INC_ADULT);

constexpr S32 MAX_CLASSIFIEDS = 100;

// This function is used in Adult-flag-aware viewers to pack old query flags
// into the request so that they can talk to old dataservers properly. When all
// OpenSim servers will be able to deal with adult flags, we can revert back to
// ClassifiedFlags pack_classified_flags and get rider of this one.
ClassifiedFlags pack_classified_flags_request(bool auto_renew, bool is_pg,
											  bool is_mature, bool is_adult);

ClassifiedFlags pack_classified_flags(bool auto_renew, bool is_pg,
									  bool is_mature, bool is_adult);

LL_INLINE bool is_cf_mature(ClassifiedFlags flags)
{
	return (flags & CLASSIFIED_FLAG_MATURE) != 0 ||
		   (flags & CLASSIFIED_QUERY_INC_MATURE) != 0;
}

LL_INLINE bool is_cf_update_time(ClassifiedFlags flags)
{
	return (flags & CLASSIFIED_FLAG_UPDATE_TIME) != 0;
}

LL_INLINE bool is_cf_auto_renew(ClassifiedFlags flags)
{
	return (flags & CLASSIFIED_FLAG_AUTO_RENEW) != 0;
}

# if 0	// Deprecated, but leaving commented out because someday we might
		// want to let users enable/disable classifieds. JC
LL_INLINE bool is_cf_enabled(ClassifiedFlags flags)
{
	return (flags & CLASSIFIED_FLAG_ENABLED) == CLASSIFIED_FLAG_ENABLED;
}
# endif

#endif	// LL_LLCLASSIFIEDFLAGS_H
