/**
 * @file hbfloaterdebugtags.h
 * @brief The HBFloaterDebugTags class declaration
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Henri Beauchamp
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

#ifndef LL_HBFLOATERDEBUGTAGS_H
#define LL_HBFLOATERDEBUGTAGS_H

#include <set>

#include "llfloater.h"

class LLScrollListCtrl;

class HBFloaterDebugTags final : public LLFloater,
								 public LLFloaterSingleton<HBFloaterDebugTags>
{
	friend class LLUISingleton<HBFloaterDebugTags,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterDebugTags);

public:
	LL_INLINE static bool hasActiveDebugTags()
	{
		return !sAddedTagsList.empty();
	}

	LL_INLINE static bool debugTagActive(const std::string& tag)
	{
		return sAddedTagsList.count(tag) != 0;
	}

	static void primeTagsFromLogControl();
	static void setTag(const std::string& tag, bool enable);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterDebugTags(const LLSD&);

	bool postBuild() override;
	void draw() override;

	void refreshList();

	static void onSelectLine(LLUICtrl* ctrl, void* data);

private:
	LLScrollListCtrl*				mDebugTagsList;
	bool							mIsDirty;

	static std::set<std::string>	sDefaultTagsList;
	static std::set<std::string>	sAddedTagsList;
};

#endif
