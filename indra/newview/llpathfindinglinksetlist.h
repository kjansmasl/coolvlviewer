/** 
 * @file llpathfindinglinksetlist.h
 * @brief Header file for llpathfindinglinksetlist
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLPATHFINDINGLINKSETLIST_H
#define LL_LLPATHFINDINGLINKSETLIST_H

#include "llpathfindinglinkset.h"
#include "llpathfindingobjectlist.h"

class LLSD;

class LLPathfindingLinksetList : public LLPathfindingObjectList
{
protected:
	LOG_CLASS(LLPathfindingLinksetList);

public:
	LLPathfindingLinksetList();
	LLPathfindingLinksetList(const LLSD& data);

	LL_INLINE LLPathfindingLinksetList* asLinksetList() override
	{
		return this;
	}

	LL_INLINE const LLPathfindingLinksetList* asLinksetList() const override
	{
		return this;
	}

	typedef LLPathfindingLinkset::ELinksetUse EUsage;

	LLSD encodeObjectFields(EUsage use, S32 a, S32 b, S32 c, S32 d) const;
	LLSD encodeTerrainFields(EUsage use, S32 a, S32 b, S32 c, S32 d) const;

	bool showUnmodifiablePhantomWarning(EUsage use) const;
	bool showPhantomToggleWarning(EUsage use) const;
	bool showCannotBeVolumeWarning(EUsage use) const;

	void determinePossibleStates(bool& walkable, bool& static_obstacle,
								 bool& dynamic_obstacle, bool& material_volume,
								 bool& exclusion_volume,
								 bool& dynamic_phantom) const;

private:
	void parseLinksetListData(const LLSD& data);
};

#endif // LL_LLPATHFINDINGLINKSETLIST_H
