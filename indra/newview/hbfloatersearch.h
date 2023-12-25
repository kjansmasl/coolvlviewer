/**
 * @file hbfloatersearch.h
 * @brief The "Search" floater definition.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc. (c) 2009-2021, Henri Beauchamp.
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

// This used to be LL's v1 viewer "Directory" floater (llfloaterdirectory.h),
// and was modified/expanded by Henri Beauchamp to add web search, showcase and
// Marketplace when LL added them to their own v2+ viewers, as well as support
// for OpenSim grids (optional) web search.

#ifndef LL_HBFLOATERSEARCH_H
#define LL_HBFLOATERSEARCH_H

#include "llfloater.h"

class LLPanelDirClassified;
class LLPanelDirEvents;
class LLPanelDirFind;
class LLPanelDirLand;
class HBPanelWebSearch;

class LLPanelAvatar;
class LLPanelEvent;
class LLPanelGroup;
class LLPanelPlace;
class LLPanelClassified;
class LLTabContainer;

class HBFloaterSearch final : public LLFloater,
							  public LLFloaterSingleton<HBFloaterSearch>
{
	friend class LLUISingleton<HBFloaterSearch,
							   VisibilityPolicy<LLFloater> >;

public:
	~HBFloaterSearch() override;

	void setVisible(bool visible) override;

	void hideAllDetailPanels();

	// Outside UI widgets can spawn this floater with various tabs selected.
	static void showFindAll(const std::string& search_text);
	static void showClassified(const LLUUID& classified_id);
	static void showEvents(S32 event_id);
	static void showLandForSale(const LLUUID& parcel_id);
	static void showGroups();

	static void toggle();

	static void	setSearchURL(const std::string& url, bool on_login = false);
	static bool wasSearchURLSetOnLogin();

	// Used for toggling God mode, which changes to visibility of some picks.
	static void requestClassifieds();

	static void refreshGroup(const LLUUID& group_id);

private:
	// Open only via the show*() or toggle() static methods defined above.
	HBFloaterSearch(const LLSD&);

	void focusCurrentPanel();

	static void onTabChanged(void*, bool);

	static void onTeleportArriving();
	static void showPanel(const std::string& tabname);

	static void* createFindAll(void* userdata);
	static void* createClassified(void* userdata);
	static void* createEvents(void* userdata);
	static void* createPlaces(void* userdata);
	static void* createLand(void* userdata);
	static void* createPeople(void* userdata);
	static void* createGroups(void* userdata);
	static void* createWebSearch(void* userdata);

	static void* createClassifiedDetail(void* userdata);
	static void* createAvatarDetail(void* userdata);
	static void* createEventDetail(void* userdata);
	static void* createGroupDetail(void* userdata);
	static void* createGroupDetailHolder(void* userdata);
	static void* createPlaceDetail(void* userdata);
	static void* createPlaceDetailSmall(void* userdata);
	static void* createPanelAvatar(void* userdata);

public:
	LLPanelAvatar*				mPanelAvatarp;
	LLPanelEvent*				mPanelEventp;
	LLPanelGroup*				mPanelGroupp;
	LLPanel*					mPanelGroupHolderp;
	LLPanelPlace*				mPanelPlacep;
	LLPanelPlace*				mPanelPlaceSmallp;
	LLPanelClassified*			mPanelClassifiedp;

private:
	boost::signals2::connection	mTeleportArrivingConnection;

	LLPanelDirFind*				mFindAllPanel;
	LLPanelDirClassified*		mClassifiedPanel;
	LLPanelDirEvents*			mEventsPanel;
	LLPanelDirLand*				mLandPanel;
	HBPanelWebSearch*			mSearchWebPanel;

	LLTabContainer*				mTabsContainer;

	static bool					sSearchURLSetOnLogin;
};

extern bool gDisplayEventHack;

#endif  // LL_HBFLOATERSEARCH_H
