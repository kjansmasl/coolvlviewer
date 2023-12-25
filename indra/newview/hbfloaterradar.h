/**
 * @file hbfloaterradar.h
 * @brief Radar floater class definition
 *
 * @authors Original code from Dale Glass, amended by jcool410 for Emerald,
 * fully rewritten and heavily modified by Henri Beauchamp (the laggy spying
 * tool becomes a true, low lag radar), with avatar marking and announcements
 * additions, among others.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2008-2020, Henri Beauchamp.
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

#ifndef LL_HBFLOATERRADAR_H
#define LL_HBFLOATERRADAR_H

#include "llfloater.h"
#include "llframetimer.h"
#include "lltimer.h"

#include "llavatartracker.h"
#include "llmutelist.h"

class LLButton;
class LLCheckBoxCtrl;
class HBFloaterRadar;
class LLScrollListCtrl;
class LLTabContainer;
class LLVOAvatar;

// This class is used to cache data about avatars. Instances are kept in an
// unordered map. We keep track of the frame where the avatar was last seen.
class HBRadarListEntry
{
	friend class HBFloaterRadar;

public:
	HBRadarListEntry(LLVOAvatar* avatarp, const LLUUID& avid,
					 const std::string& name, const std::string& display_name,
					 const LLVector3d& position, bool marked);

	// We delete the copy constructor for performances (as a result all entries
	// will have to be constructed emplace in the avatar list).
	HBRadarListEntry(const HBRadarListEntry&) = delete;

	~HBRadarListEntry();

	// Update world position. Affects age.
	void setPosition(const LLVector3d& position, bool this_sim, bool drawn,
					 bool chatrange, bool shoutrange);

	LL_INLINE const LLVector3d& getPosition() const	{ return mPosition; }

	// This is only used for determining whether the avatar is still around:
	// see getEntryAgeSeconds()
	bool getAlive();

	// Returns the age of this entry in seconds
	LL_INLINE F32 getEntryAgeSeconds() const
	{
		return mUpdateTimer.getElapsedTimeF32();
	}

	// Returns the ID of the avatar
	LL_INLINE const LLUUID& getID()	const			{ return mID; }

	// Returns the name of the avatar
	LL_INLINE const std::string& getName() const	{ return mName; }

	// Sets the display name of the avatar
	LL_INLINE void setDisplayName(const std::string& name)
	{
		mDisplayName = name;
	}

	// Returns the display name of the avatar
	LL_INLINE const std::string& getDisplayName() const
	{
		return mDisplayName;
	}

	LL_INLINE bool isMuted() const					{ return mMuted; }
	LL_INLINE bool isDerendered() const				{ return mDerendered; }
	LL_INLINE bool isFriend() const					{ return mFriend; }

	// Sets the 'focus' status on this entry (camera focused on this avatar)
	LL_INLINE void setFocus(bool value)				{ mFocused = value; }

	LL_INLINE bool isFocused() const				{ return mFocused; }

	LL_INLINE bool isMarked() const					{ return mMarked; }

	LL_INLINE bool isCustomMark() const				{ return mCustomMark; }

	LL_INLINE bool isDrawn() const					{ return mInDrawFrame != U32_MAX; }

	LL_INLINE bool isInSim() const					{ return mInSimFrame != U32_MAX; }

	// Returns true when the item is dead and should not appear in the list
	LL_INLINE bool isDead() const
	{
		// How long to keep people who are gone in the list and in memory.
		constexpr F32 DEAD_KEEP_TIME = 10.f;
		return getEntryAgeSeconds() > DEAD_KEEP_TIME;
	}

	LL_INLINE void setColor(const LLColor4& col)	{ mColor = col; }
	LL_INLINE const LLColor4& getColor() const		{ return mColor; }

	LL_INLINE void setMarkColor(const LLColor4& c)	{ mMarkColor = c; }
	LL_INLINE const LLColor4& getMarkColor() const	{ return mMarkColor; }

	LL_INLINE void setToolTip(const std::string& text)
	{
		mToolTip = text;
	}

	LL_INLINE const std::string& getToolTip() const
	{
		return mToolTip;
	}

	// Returns true if successful (mark is valid and set/displayed) or false
	// otherwise (with the mark reset to "X"). Note that reset the custom mark
	// (and stop displaying it in the radar if the avatar was not marked by the
	// user), you simply need to pass an empty string to this method.
	bool setMarkChar(const std::string& chr);

	LL_INLINE const std::string& getMarkChar() const
	{
		return mMarkChar;
	}

	LL_INLINE bool toggleMark()						{ mMarked = !mMarked; return mMarked; }
	LL_INLINE void setMarked()						{ mMarked = true; }

private:
	typedef enum e_radar_alert_type
	{
		ALERT_TYPE_SIM = 0,
		ALERT_TYPE_DRAW = 1,
		ALERT_TYPE_SHOUTRANGE = 2,
		ALERT_TYPE_CHATRANGE = 3
	} ERadarAlertType;

	// Emits announcements about the avatar entering or leaving the various
	// ranges, in chat and/or private channel. Returns true when something was
	// actually posted in chat.
	bool reportAvatarStatus(ERadarAlertType type, bool entering);

private:
	LLUUID		mID;
	std::string	mName;
	std::string	mDisplayName;
	std::string	mToolTip;
	std::string	mMarkChar;
	LLColor4	mColor;
	LLColor4	mMarkColor;
	LLVector3d	mPosition;
	LLVector3d	mDrawPosition;

	// Timer to keep track of whether avatars are still there
	LLTimer		mUpdateTimer;

	// Last frame when this avatar was updated
	U32			mFrame;
	// Last frame when this avatar was in sim
	U32			mInSimFrame;
	// Last frame when this avatar was in draw
	U32			mInDrawFrame;
	// Last frame when this avatar was in shout range
	U32			mInShoutFrame;
	// Last frame when this avatar was in chat range
	U32			mInChatFrame;

	bool		mMuted;
	bool		mDerendered;
	bool		mFriend;
	bool		mMarked;
	bool		mCustomMark;
	bool		mFocused;
};

class HBFloaterRadar final : public LLMuteListObserver,
							 public LLFriendObserver,
							 public LLFloater,
					 		 public LLFloaterSingleton<HBFloaterRadar>
{
	friend class LLUISingleton<HBFloaterRadar, VisibilityPolicy<LLFloater> >;
	friend class HBRadarListEntry;	// For access to static cached strings

protected:
	LOG_CLASS(HBFloaterRadar);

	void onClose(bool app_quitting) override;
	void onOpen() override;
	bool postBuild() override;

	// Mute list observer inteface
	void onChange() override;
	// Friends list observer inteface
	void changed(U32 mask) override;

public:
	~HBFloaterRadar() override;

	// Returns the entry for an avatar, if preset, NULL if not found.
	HBRadarListEntry* getAvatarEntry(const LLUUID& avid);

	// Returns a string with the selected names in the list
	std::string getSelectedNames(const std::string& separator = ", ");

	std::string getSelectedName();

	LLUUID getSelectedID();

	LL_INLINE bool isAvatarMarked(const LLUUID& avid)
	{
		return mMarkedAvatars.count(avid) > 0;
	}

	bool startTracker(const LLUUID& avid);	// Returns true on success
	void stopTracker();

	// Sets the color for the avatar name in the list; returns true when
	// successful (i.e. if the avatar is found in the currently active radar
	// list, or there is no open/running radar floater).
	static bool setAvatarNameColor(const LLUUID& avid, const LLColor4& color);

	// Updates the render status for a given avatar, or for all avatars if
	// avid is omitted or null. To use each time you modify the derendered
	// objects list (LLViewerObjectList::sBlackListedObjects) by adding or
	// removing an avatar. Using such a method prevents from having to
	// check each listed avatar render status at each avatar list update.
	static void setRenderStatusDirty(const LLUUID& avid = LLUUID::null);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterRadar(const LLSD&);

	// Updates the avatars list with the surounding avatars.
	void updateAvatarList();

	// Cleans up the avatars list, removing dead entries from it. This lets
	// dead entries remain for some time so that it is possible to trigger
	// actions on avtars passing by in the list.
	void expireAvatarList();

	enum AVATARS_COLUMN_ORDER
	{
		LIST_MARK,
		LIST_AVATAR_NAME,
		LIST_DISTANCE,
		LIST_POSITION,
		LIST_ALTITUDE
	};

	// Refreshes the avatars list
	void refreshAvatarList();

	// Loading and saving of the marked avatars list
	bool loadMarkedFromFile();
	bool saveMarkedToFile(bool force = false);

	// Removes focus status from all avatars in list
	void removeFocusFromAll();

	// Focuses the camera on current avatar
	void focusOnCurrent();

	// Focuses the camera on the previous avatar (marked ones only when
	// marked_only is true)
	void focusOnPrev(bool marked_only);

	// Focuses the camera on the next avatar (marked ones only when marked_only
	// is true)
	void focusOnNext(bool marked_only);

	void refreshTracker();

	static void connectRefreshCachedSettingsSafe(const char* name);
	static void refreshCachedSettings();

	static void onTabChanged(void* userdata, bool from_click);

	static void onClickProfile(void* userdata);
	static void onClickTrack(void* userdata);
	static void onClickIM(void* userdata);
	static void onClickTeleportOffer(void* userdata);
	static void onClickTeleportTo(void* userdata);
	static void onClickTeleportRequest(void* userdata);
	static void onDoubleClick(void* userdata);
	static void onClickFocus(void* userdata);
	static void onClickPrevInList(void* userdata);
	static void onClickNextInList(void* userdata);
	static void onClickMark(void* userdata);
	static void onClickPrevMarked(void* userdata);
	static void onClickNextMarked(void* userdata);

	static void onCheckRadarAlerts(LLUICtrl* ctrl, void* userdata);
	static void onCheckUseLegacyNames(LLUICtrl* ctrl, void* userdata);

	static void onClickFreeze(void* userdata);
	static void onClickEject(void* userdata);
	static void onClickMute(void* userdata);
	static void onClickDerender(void* userdata);
	static void onClickAR(void* userdata);
	static void onClickEjectFromEstate(void* userdata);
	static void onClickGetKey(void* userdata);
	static void onClickClearSavedMarked(void* userdata);

	static void callbackFreeze(const LLSD& notification, const LLSD& response);
	static void callbackEject(const LLSD& notification, const LLSD& response);
	static void callbackAR(void* userdata);
	static void callbackEjectFromEstate(const LLSD& notification,
										const LLSD& response);

	static void onSelectName(LLUICtrl* ctrlp, void* userdata);

	static void onClickSendKeys(void* userdata);

	static void callbackIdle(void* userdata);

	typedef void (*avlist_command_t)(const LLUUID& avid);
	void doCommand(avlist_command_t cmd);

private:
	uuid_vec_t				mLastSelection;

	LLTabContainer*			mTabContainer;

	LLScrollListCtrl*		mAvatarList;

	LLButton*				mProfileButton;
	LLButton*				mTrackButton;
	LLButton*				mIMButton;
	LLButton*				mTPOfferButton;
	LLButton*				mRequestTPButton;
	LLButton*				mTeleportToButton;
	LLButton*				mMarkButton;
	LLButton*				mPrevMarkedButton;
	LLButton*				mNextMarkedButton;
	LLButton*				mFocusButton;
	LLButton*				mPrevInListButton;
	LLButton*				mNextInListButton;
	LLButton*				mMuteButton;
	LLButton*				mFreezeButton;
	LLButton*				mARButton;
	LLButton*				mEjectButton;
	LLButton*				mEstateEjectButton;
	LLButton*				mGetKeyButton;
	LLButton*				mDerenderButton;
	LLButton*				mRerenderButton;
	LLButton*				mClearSavedMarkedButton;

	LLCheckBoxCtrl*			mRadarAlertsCheck;
	LLCheckBoxCtrl*			mSimAlertsCheck;
	LLCheckBoxCtrl*			mDrawAlertsCheck;
	LLCheckBoxCtrl*			mShoutAlertsCheck;
	LLCheckBoxCtrl*			mChatAlertsCheck;
	LLCheckBoxCtrl*			mUseLegacyNamesCheck;

	typedef safe_hmap<LLUUID, HBRadarListEntry> avatar_list_t;
	avatar_list_t			mAvatars;

	uuid_list_t				mMarkedAvatars;

	LLUUID					mFocusedAvatar;	// Avatar the camera is focused on

	// Tracking data
	LLUUID					mTrackedAvatar;	// Who we are tracking
	bool					mTracking;		// Tracking ?

	static LLFrameTimer		sUpdateTimer;	// Update timer

	// Cached UI strings
	static std::string		sTotalAvatarsStr;
	static std::string		sNoAvatarStr;
	static std::string		sLastKnownPosStr;

protected:
	// Cached UI strings, colors and settings
	static std::string		sCardinals;
	static std::string		sHasEnteredStr;
	static std::string		sHasLeftStr;
	static std::string		sTheSimStr;
	static std::string		sDrawDistanceStr;
	static std::string		sShoutRangeStr;
	static std::string		sChatRangeStr;
	static LLColor4			sMarkColor;
	static LLColor4			sNameColor;
	static LLColor4			sFriendNameColor;
	static LLColor4			sMutedNameColor;
	static LLColor4			sDerenderedNameColor;
	static LLColor4			sFarDistanceColor;
	static LLColor4			sShoutDistanceColor;
	static LLColor4			sChatDistanceColor;
	// Update rate (updates per second)
	static U32				sUpdatesPerSecond;
	static bool				sRememberMarked;
};

#endif	// LL_HBFLOATERRADAR_H
