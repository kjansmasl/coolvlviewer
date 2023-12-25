/**
 * @file hbfloaterareasearch.h
 * @brief HBFloaterAreaSearch class definition
 *
 * This class implements a floater where all surroundind objects are listed.
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Original code Copyright (c) 2009 Modular Systems Ltd. All rights reserved.
 * Rewritten/augmented code Copyright (c) 2010-2019 Henri Beauchamp.
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
 *
 * Revision history:
 * - Initial backport from Emerald viewer, modified, debugged, optimized and
 *   improved by Henri Beauchamp - Feb 2010.
 * - Speed optimization by Henri Beauchamp - Jul 2011.
 * - Tracked object info added by Henri Beauchamp - Nov 2011.
 * - Further modified and augmented with mutes, derender, report & inspect
 *   functions. Henri Beauchamp - May 2013.
 * - Full rewrite by Henri Beauchamp - Aug 2014.
 * - Another partial rewrite by Henri Beauchamp - Nov 2016.
 * - Yet another large rewrite, with proper exclusion of attachments and
 *   viewer-side only objects to avoid fake "pending" requests and to prevent
 *   sending to the server the bogus/spammy/useless messages corresponding to
 *   them. Fixed the bug that caused group-owned objects to stay forever in the
 *   "pending" requests list. Added the "Show", "Mute particles", "Mute owner"
 *   and "Copy UUID" action. Changed input lines for search input lines.
 *   Henri Beauchamp - Jan 2019.
 * - On "Refresh", clear the cached objects list so that any renamed object
 *   will be properly refreshed. Henri Beauchamp - Apr 2021.
 */

#ifndef LL_HBFLOATERAREASEARCH_H
#define LL_HBFLOATERAREASEARCH_H

#include "llfloater.h"

class LLButton;
class LLFlyoutButton;
class LLMessageSystem;
class LLScrollListCtrl;
class LLSearchEditor;
class LLTextBox;
class LLViewerObject;

class HBFloaterAreaSearch final : public LLFloater,
								  public LLFloaterSingleton<HBFloaterAreaSearch>
{
	friend class LLUISingleton<HBFloaterAreaSearch,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterAreaSearch);

public:
	// Used in llappviewer.cpp to trigger idle updates to background object
	// properties fetches.
	static void idleUpdate();

	// Used in llviewermessage.cpp to inform us we changed region
	static void newRegion();

	// Called from llviewermessage.cpp, in the callback for the
	// RequestObjectPropertiesFamily message reply.
	static void processObjectPropertiesFamily(LLMessageSystem* msg);

	static void setDirty()							{ sIsDirty = true; }

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterAreaSearch(const LLSD&);

	bool postBuild() override;
	void draw() override;

	enum OBJECT_COLUMN_ORDER
	{
		LIST_OBJECT_NAME,
		LIST_OBJECT_DESC,
		LIST_OBJECT_OWNER,
		LIST_OBJECT_GROUP
	};

	void addInResultsList(const LLUUID& object_id, bool match_filters);

	void setButtonsStatus();

	// Returns true when objectp is not NULL, not a viewer-side object (cloud,
	// particle, sky, surface patch, etc), not an avatar, a root primitive, not
	// temporary and not an attachment.
	static bool isObjectOfInterest(LLViewerObject* objectp);

	// Returns true if the object details are up to date, false otherwise and
	// in that latter case, sends an update request if needed.
	static bool checkObjectDetails(const LLUUID& object_id);

	static void onSelectResult(LLUICtrl*, void* userdata);
	static void onDoubleClickResult(void* userdata);
	static void onClickRefresh(void*);
	static void onClickClose(void* userdata);
	static void onClickDerender(void* userdata);
	static void onClickReport(void* userdata);
	static void onClickShow(void* userdata);
	static void onClickMute(LLUICtrl* ctrl, void* userdata);
	static void onClickInspect(LLUICtrl* ctrl, void* userdata);
	static void onSearchEdit(const std::string& search_string, void* userdata);

	class HBObjectDetails
	{
	public:
		HBObjectDetails()
		:	time_stamp(-10000.f)
		{
		}

		LL_INLINE bool valid()
		{
			return owner_id.notNull() || group_id.notNull();
		}

	public:
		F32			time_stamp;
		LLUUID		owner_id;
		LLUUID		group_id;
		std::string	name;
		std::string	desc;
	};

private:
	LLTextBox*					mCounterText;
	LLScrollListCtrl*			mResultsList;
	LLSearchEditor*				mNameInputLine;
	LLSearchEditor*				mDescInputLine;
	LLSearchEditor*				mOwnerInputLine;
	LLSearchEditor*				mGroupInputLine;
	LLFlyoutButton*				mMuteFlyoutBtn;
	LLFlyoutButton*				mInspectFlyoutBtn;
	LLButton*					mDerenderBtn;
	LLButton*					mReportBtn;
	LLButton*					mShowBtn;
	LLButton*					mRefreshBtn;

	LLUUID						mSearchUUID;
	std::string					mSearchedName;
	std::string					mSearchedDesc;
	std::string					mSearchedOwner;
	std::string					mSearchedGroup;

	static F32					sLastUpdateTime;
	static bool					sIsDirty;
	static bool					sUpdateDone;

	static bool					sTracking;
	static LLUUID				sTrackingObjectID;
	static LLVector3d			sTrackingLocation;
	static std::string			sTrackingInfoLine;

	typedef fast_hmap<LLUUID, HBObjectDetails> object_details_map_t;
	static object_details_map_t	sObjectDetails;
};

#endif // LL_HBFLOATERAREASEARCH_H
