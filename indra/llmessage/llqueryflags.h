/**
 * @file llqueryflags.h
 * @brief Flags for directory queries
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLQUERYFLAGS_H
#define LL_LLQUERYFLAGS_H

// Binary flags used for Find queries, shared between viewer and dataserver.

// DirFindQuery flags
constexpr U32 DFQ_PEOPLE			= 0x1 << 0;
constexpr U32 DFQ_ONLINE			= 0x1 << 1;
//constexpr U32 DFQ_PLACES			= 0x1 << 2;
constexpr U32 DFQ_EVENTS			= 0x1 << 3;
constexpr U32 DFQ_GROUPS			= 0x1 << 4;
constexpr U32 DFQ_DATE_EVENTS		= 0x1 << 5;

constexpr U32 DFQ_AGENT_OWNED		= 0x1 << 6;
constexpr U32 DFQ_FOR_SALE			= 0x1 << 7;
constexpr U32 DFQ_GROUP_OWNED		= 0x1 << 8;
//constexpr U32 DFQ_AUCTION			= 0x1 << 9;
constexpr U32 DFQ_DWELL_SORT		= 0x1 << 10;
constexpr U32 DFQ_PG_SIMS_ONLY		= 0x1 << 11;
constexpr U32 DFQ_PICTURES_ONLY		= 0x1 << 12;
constexpr U32 DFQ_PG_EVENTS_ONLY	= 0x1 << 13;
constexpr U32 DFQ_MATURE_SIMS_ONLY  = 0x1 << 14;

constexpr U32 DFQ_SORT_ASC			= 0x1 << 15;
constexpr U32 DFQ_PRICE_SORT		= 0x1 << 16;
constexpr U32 DFQ_PER_METER_SORT	= 0x1 << 17;
constexpr U32 DFQ_AREA_SORT			= 0x1 << 18;
constexpr U32 DFQ_NAME_SORT			= 0x1 << 19;

constexpr U32 DFQ_LIMIT_BY_PRICE	= 0x1 << 20;
constexpr U32 DFQ_LIMIT_BY_AREA		= 0x1 << 21;

constexpr U32 DFQ_FILTER_MATURE		= 0x1 << 22;
constexpr U32 DFQ_PG_PARCELS_ONLY	= 0x1 << 23;

constexpr U32 DFQ_INC_PG		 	= 0x1 << 24;
constexpr U32 DFQ_INC_MATURE     	= 0x1 << 25;
constexpr U32 DFQ_INC_ADULT     	= 0x1 << 26;

constexpr U32 DFQ_ADULT_SIMS_ONLY	= 0x1 << 27;

// Sell Type flags
constexpr U32 ST_AUCTION	= 0x1 << 1;
constexpr U32 ST_NEWBIE		= 0x1 << 2;
constexpr U32 ST_MAINLAND	= 0x1 << 3;
constexpr U32 ST_ESTATE		= 0x1 << 4;

constexpr U32 ST_ALL		= 0xFFFFFFFF;

// Status flags embedded in search replay messages of classifieds, events,
// groups, and places.
// Places
constexpr U32 STATUS_SEARCH_PLACES_NONE				= 0x0;
constexpr U32 STATUS_SEARCH_PLACES_BANNEDWORD		= 0x1 << 0;
constexpr U32 STATUS_SEARCH_PLACES_SHORTSTRING		= 0x1 << 1;
constexpr U32 STATUS_SEARCH_PLACES_FOUNDNONE		= 0x1 << 2;
constexpr U32 STATUS_SEARCH_PLACES_SEARCHDISABLED	= 0x1 << 3;
constexpr U32 STATUS_SEARCH_PLACES_ESTATEEMPTY		= 0x1 << 4;
// Events
constexpr U32 STATUS_SEARCH_EVENTS_NONE				= 0x0;
constexpr U32 STATUS_SEARCH_EVENTS_BANNEDWORD		= 0x1 << 0;
constexpr U32 STATUS_SEARCH_EVENTS_SHORTSTRING		= 0x1 << 1;
constexpr U32 STATUS_SEARCH_EVENTS_FOUNDNONE		= 0x1 << 2;
constexpr U32 STATUS_SEARCH_EVENTS_SEARCHDISABLED	= 0x1 << 3;
constexpr U32 STATUS_SEARCH_EVENTS_NODATEOFFSET		= 0x1 << 4;
constexpr U32 STATUS_SEARCH_EVENTS_NOCATEGORY		= 0x1 << 5;
constexpr U32 STATUS_SEARCH_EVENTS_NOQUERY			= 0x1 << 6;

// Classifieds
constexpr U32 STATUS_SEARCH_CLASSIFIEDS_NONE			= 0x0;
constexpr U32 STATUS_SEARCH_CLASSIFIEDS_BANNEDWORD		= 0x1 << 0;
constexpr U32 STATUS_SEARCH_CLASSIFIEDS_SHORTSTRING		= 0x1 << 1;
constexpr U32 STATUS_SEARCH_CLASSIFIEDS_FOUNDNONE		= 0x1 << 2;
constexpr U32 STATUS_SEARCH_CLASSIFIEDS_SEARCHDISABLED	= 0x1 << 3;

#endif
