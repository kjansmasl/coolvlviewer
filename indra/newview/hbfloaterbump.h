/**
 * @file hbfloaterbump.h
 * @brief Floater listing bumps, pushes and hits, and allowing to take actions.
 * @author Henri Beauchamp
 *
 * $LicenseInfo:firstyear=2020&license=viewergpl$
 *
 * Copyright (c) 2020, Henri Beauchamp
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

#ifndef LL_LLFLOATERBUMP_H
#define LL_LLFLOATERBUMP_H

#include <list>

#include "llfloater.h"

class LLButton;
class LLMeanCollisionData;
class LLScrollListCtrl;

typedef enum e_mean_collision_types
{
	MEAN_INVALID,
	MEAN_BUMP,
	MEAN_LLPUSHOBJECT,
	MEAN_SELECTED_OBJECT_COLLIDE,
	MEAN_SCRIPTED_OBJECT_COLLIDE,
	MEAN_PHYSICAL_OBJECT_COLLIDE,
	MEAN_EOF
} EMeanCollisionType;

class HBFloaterBump final : public LLFloater,
							public LLFloaterSingleton<HBFloaterBump>
{
	friend class LLUISingleton<HBFloaterBump, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterBump);

public:
	void refresh() override;

	static void cleanup();

	static void addMeanCollision(const LLUUID& id, U32 time,
								 EMeanCollisionType type, F32 mag);

	static std::string getMeanCollisionsStats(const LLUUID& perpetrator_id);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterBump(const LLSD&);

	bool postBuild() override;
	void draw() override;

	static void onButtonClear(void*);
	static void onButtonClose(void* data);
	static void onButtonFocus(void* data);
	static void onButtonProfile(void* data);
	static void onButtonReport(void* data);

	static void meanNameCallback(const LLUUID& id, const std::string& fullname,
								 bool);
private:
	LLScrollListCtrl*			mBumpsList;
	LLButton*					mClearButton;
	LLButton*					mFocusButton;
	LLButton*					mProfileButton;
	LLButton*					mReportButton;

	typedef std::list<LLMeanCollisionData> collisions_list_t;
	static collisions_list_t	sMeanCollisionsList;
	static bool					sListUpdated;
};

#endif
