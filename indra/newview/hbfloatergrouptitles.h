/**
 * @file hbfloatergrouptitles.h
 * @brief HBFloaterGroupTitles class definition
 *
 * This class implements a floater where all available group titles are
 * listed, allowing the user to activate any via simple double-click.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Henri Beauchamp.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_HBFLOATERGROUPTITLES_H
#define LL_HBFLOATERGROUPTITLES_H

#include "llevent.h"
#include "llfloater.h"
#include "llgroupmgr.h"

class HBFloaterGroupTitles;
class LLScrollListCtrl;

class HBFloaterGroupTitlesObserver final : public LLGroupMgrObserver
{
public:
	HBFloaterGroupTitlesObserver(HBFloaterGroupTitles* instance,
								 const LLUUID& group_id);
	~HBFloaterGroupTitlesObserver() override;

protected:
	void changed(LLGroupChange gc) override;

private:
	HBFloaterGroupTitles* mFloaterInstance;
};

class HBFloaterGroupTitles final
:	public LLFloater,
	public LLFloaterSingleton<HBFloaterGroupTitles>,
	public LLOldEvents::LLSimpleListener
{
	friend class LLUISingleton<HBFloaterGroupTitles,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterGroupTitles);

public:
	~HBFloaterGroupTitles() override;

	void setDirty()								{ mIsDirty = true; }

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterGroupTitles(const LLSD&);

	bool postBuild() override;
	void draw() override;

	// LLSimpleListener interface
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
					 const LLSD& userdata) override;

	static void onActivate(void* data);
	static void onRefreshButtonPressed(void* data);
	static void onCloseButtonPressed(void* data);

	enum TITLES_COLUMN_ORDER
	{
		LIST_TITLE = 0,
		LIST_GROUP_NAME,
		LIST_GROUP_ID
	};

private:
	typedef fast_hmap<LLUUID, HBFloaterGroupTitlesObserver*> observers_map_t;
	observers_map_t		mObservers;
	LLScrollListCtrl*	mTitlesList;
	bool				mIsDirty;
};

#endif
