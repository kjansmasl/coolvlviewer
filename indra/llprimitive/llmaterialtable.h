/**
 * @file llmaterialtable.h
 * @brief Table of material information for the viewer UI
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

#ifndef LL_LLMATERIALTABLE_H
#define LL_LLMATERIALTABLE_H

#include "lluuid.h"
#include "llstring.h"

#include <list>

// NOTE: this is a simplified version of the material table with all the server
// related code removed (since never used by the viewer). HB

// Material types
constexpr U8 LL_MCODE_STONE   = 0;
constexpr U8 LL_MCODE_METAL   = 1;
constexpr U8 LL_MCODE_GLASS   = 2;
constexpr U8 LL_MCODE_WOOD    = 3;
constexpr U8 LL_MCODE_FLESH   = 4;
constexpr U8 LL_MCODE_PLASTIC = 5;
constexpr U8 LL_MCODE_RUBBER  = 6;
constexpr U8 LL_MCODE_LIGHT   = 7;
constexpr U8 LL_MCODE_END     = 8;
constexpr U8 LL_MCODE_MASK    = 0x0F;

class LLMaterialInfo
{
public:
	LL_INLINE LLMaterialInfo(U8 code, const char* name, F32 fric, F32 rest)
	:	mMCode(code),
		mName(name),
		mFriction(fric),
		mRestitution(rest)
	{
	}

public:
	std::string	mName;
	F32         mFriction;
	F32         mRestitution;
	U8		    mMCode;
};

class LLMaterialTable
{
public:
	LLMaterialTable();

	typedef std::map<std::string, std::string> name_map_t;
	void initTableTransNames(name_map_t namemap);

	U8 getMCode(const std::string& name) const;	// 0 if not found
	const std::string& getName(U8 mcode) const;

	// Physics values (used as default in the Build tools floater)
	F32 getFriction(U8 mcode) const;
	F32 getRestitution(U8 mcode) const;

	LL_INLINE bool isCollisionSound(const LLUUID& sound_id) const
	{
		return mCollisionsSounds.count(sound_id);
	}

public:
	typedef std::list<LLMaterialInfo> info_list_t;
	info_list_t				mMaterialInfoList;

	uuid_list_t				mCollisionsSounds;
};

extern LLMaterialTable gMaterialTable;

#endif
