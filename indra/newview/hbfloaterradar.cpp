/**
 * @file hbfloaterradar.cpp
 * @brief Radar floater class implementation
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

#include "llviewerprecompiledheaders.h"

#include <utility>

#include "hbfloaterradar.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llradiogroup.h"
#include "llregionflags.h"
#include "llscrolllistctrl.h"
#include "llsdutil.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameCount
#include "llavataractions.h"
#include "llchat.h"
#include "llfloateravatarinfo.h"
#include "llfloaterchat.h"
#include "llfloaterinspect.h"
#include "llfloatermute.h"
#include "llfloaterreporter.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltracker.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llworld.h"

#define COMMENT_PREFIX "\342\200\243 "

// Static member variables
LLFrameTimer HBFloaterRadar::sUpdateTimer;
std::string HBFloaterRadar::sCardinals;
std::string HBFloaterRadar::sTotalAvatarsStr;
std::string HBFloaterRadar::sNoAvatarStr;
std::string HBFloaterRadar::sLastKnownPosStr;
std::string HBFloaterRadar::sHasEnteredStr;
std::string HBFloaterRadar::sHasLeftStr;
std::string HBFloaterRadar::sTheSimStr;
std::string HBFloaterRadar::sDrawDistanceStr;
std::string HBFloaterRadar::sShoutRangeStr;
std::string HBFloaterRadar::sChatRangeStr;
LLColor4 HBFloaterRadar::sMarkColor;
LLColor4 HBFloaterRadar::sNameColor;
LLColor4 HBFloaterRadar::sFriendNameColor;
LLColor4 HBFloaterRadar::sMutedNameColor;
LLColor4 HBFloaterRadar::sDerenderedNameColor;
LLColor4 HBFloaterRadar::sFarDistanceColor;
LLColor4 HBFloaterRadar::sShoutDistanceColor;
LLColor4 HBFloaterRadar::sChatDistanceColor;
U32 HBFloaterRadar::sUpdatesPerSecond;
bool HBFloaterRadar::sRememberMarked;

// Helper function
static void announce(std::string msg)
{
	static LLCachedControl<S32> chan(gSavedSettings, "RadarChatKeysChannel");
	LL_DEBUGS("Radar") << "Radar broadcasting avatar key: " << msg
					   << " - on channel: " << (S32)chan << LL_ENDL;
	LLMessageSystem* msgsys = gMessageSystemp;
	msgsys->newMessage(_PREHASH_ScriptDialogReply);
	msgsys->nextBlock(_PREHASH_AgentData);
	msgsys->addUUID(_PREHASH_AgentID, gAgentID);
	msgsys->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msgsys->nextBlock(_PREHASH_Data);
	msgsys->addUUID(_PREHASH_ObjectID, gAgentID);
	msgsys->addS32(_PREHASH_ChatChannel, (S32)chan);
	msgsys->addS32(_PREHASH_ButtonIndex, 1);
	msgsys->addString(_PREHASH_ButtonLabel, msg);
	gAgent.sendReliableMessage();
}

///////////////////////////////////////////////////////////////////////////////
// HBRadarListEntry class
///////////////////////////////////////////////////////////////////////////////

HBRadarListEntry::HBRadarListEntry(LLVOAvatar* avatarp, const LLUUID& avid,
								   const std::string& name,
								   const std::string& display_name,
								   const LLVector3d& position, bool marked)
:	mID(avid),
	mName(name),
	mDisplayName(display_name),
	mPosition(position),
	mMarked(marked),
	mCustomMark(false),
	mMarkChar("X"),
	mMarkColor(HBFloaterRadar::sMarkColor),
	mFocused(false),
	mMuted(LLMuteList::isMuted(avid)),
	mDerendered(LLViewerObjectList::sBlackListedObjects.count(avid)),
	mFriend(LLAvatarTracker::isAgentFriend(avid)),
	mFrame(gFrameCount),
	mInSimFrame(U32_MAX),
	mInDrawFrame(U32_MAX),
	mInChatFrame(U32_MAX),
	mInShoutFrame(U32_MAX)
{
	if (avatarp)
	{
		mColor = avatarp->getRadarColor();
	}
	if (gAutomationp)
	{
		gAutomationp->onRadar(mID, mName, -1, mMarked);
	}
}

HBRadarListEntry::~HBRadarListEntry()
{
	if (gAutomationp)
	{
		gAutomationp->onRadar(mID, mName, -2, mMarked);
	}
}

bool HBRadarListEntry::setMarkChar(const std::string& chr)
{
	size_t len = chr.size();
	if (len == 0 || len > 3)	// Accept UTF-8 characters
	{
		mMarkChar = "X";
		mCustomMark = false;
		return false;
	}
	mMarkChar = chr;
	mCustomMark = true;
	return true;
}

void HBRadarListEntry::setPosition(const LLVector3d& position, bool this_sim,
								   bool drawn, bool chatrange, bool shoutrange)
{
	if (drawn)
	{
		mDrawPosition = position;
	}
	else if (mInDrawFrame == U32_MAX)
	{
		mDrawPosition.setZero();
	}
	mPosition = position;
	mFrame = gFrameCount;

	if (this_sim)
	{
		if (mInSimFrame == U32_MAX)
		{
			reportAvatarStatus(ALERT_TYPE_SIM, true);
		}
		mInSimFrame = mFrame;
	}
	if (drawn)
	{
		if (mInDrawFrame == U32_MAX)
		{
			reportAvatarStatus(ALERT_TYPE_DRAW, true);
		}
		mInDrawFrame = mFrame;
	}
	if (chatrange)
	{
		if (mInChatFrame == U32_MAX)
		{
			if (reportAvatarStatus(ALERT_TYPE_CHATRANGE, true))
			{
				// Note: if the avatar entered the chat range, then it also
				// entered the shout range, so do not announce the latter if
				// the former has already been announced.
				if (shoutrange)
				{
					mInShoutFrame = mFrame;
				}
			}
		}
		mInChatFrame = mFrame;
	}
	if (shoutrange)
	{
		if (mInShoutFrame == U32_MAX)
		{
			reportAvatarStatus(ALERT_TYPE_SHOUTRANGE, true);
		}
		mInShoutFrame = mFrame;
	}

	mUpdateTimer.start();
}

bool HBRadarListEntry::getAlive()
{
	U32 current = gFrameCount;
	if (mInSimFrame != U32_MAX && current - mInSimFrame >= 2)
	{
		mInSimFrame = U32_MAX;
		reportAvatarStatus(ALERT_TYPE_SIM, false);
	}
	if (mInDrawFrame != U32_MAX && current - mInDrawFrame >= 2)
	{
		mInDrawFrame = U32_MAX;
		reportAvatarStatus(ALERT_TYPE_DRAW, false);
	}
	if (mInShoutFrame != U32_MAX && current - mInShoutFrame >= 2)
	{
		mInShoutFrame = U32_MAX;
		if (reportAvatarStatus(ALERT_TYPE_SHOUTRANGE, false))
		{
			// Note: if the avatar left the shout range, then it also left the
			// chat range, so do not announce the latter if the former has
			// already been announced.
			mInChatFrame = U32_MAX;
		}
	}
	if (mInChatFrame != U32_MAX && current - mInChatFrame >= 2)
	{
		mInChatFrame = U32_MAX;
		reportAvatarStatus(ALERT_TYPE_CHATRANGE, false);
	}
	return current - mFrame <= 2;
}

bool HBRadarListEntry::reportAvatarStatus(ERadarAlertType type, bool entering)
{
	static LLCachedControl<bool> do_alert(gSavedSettings, "RadarChatAlerts");
	static LLCachedControl<bool> sim_range(gSavedSettings, "RadarAlertSim");
	static LLCachedControl<bool> draw_range(gSavedSettings, "RadarAlertDraw");
	static LLCachedControl<bool> shout_range(gSavedSettings,
											 "RadarAlertShoutRange");
	static LLCachedControl<bool> chat_range(gSavedSettings,
											"RadarAlertChatRange");
	static LLCachedControl<bool> send_key(gSavedSettings, "RadarChatKeys");
	bool announced = false;
	if (do_alert)
	{
		LLChat chat;
		// *TODO: translate
		std::string message = mDisplayName + " ";
		if (entering)
		{
			message += HBFloaterRadar::sHasEnteredStr + " ";
		}
		else
		{
			message += HBFloaterRadar::sHasLeftStr + " ";
		}
		switch (type)
		{
			case ALERT_TYPE_SIM:
				if (sim_range)
				{
					chat.mText = message + HBFloaterRadar::sTheSimStr;
				}
				break;

			case ALERT_TYPE_DRAW:
				if (draw_range)
				{
					chat.mText = message + HBFloaterRadar::sDrawDistanceStr;
				}
				break;

			case ALERT_TYPE_SHOUTRANGE:
				if (shout_range)
				{
					chat.mText = message + HBFloaterRadar::sShoutRangeStr;
				}
				break;

			case ALERT_TYPE_CHATRANGE:
				if (chat_range)
				{
					chat.mText = message + HBFloaterRadar::sChatRangeStr;
				}
		}
		if (!chat.mText.empty())
		{
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChat(chat);
			announced = true;
		}
	}
	if (send_key && entering && type == ALERT_TYPE_SIM)
	{
		announce(mID.asString());
	}
	if (entering && gAutomationp)
	{
		gAutomationp->onRadar(mID, mName, type, mMarked);
	}
	return announced;
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterRadar class proper
///////////////////////////////////////////////////////////////////////////////

HBFloaterRadar::HBFloaterRadar(const LLSD&)
:	mTracking(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_radar.xml");
	LLMuteList::addObserver(this);
	gAvatarTracker.addObserver(this);
}

//virtual
HBFloaterRadar::~HBFloaterRadar()
{
	LLMuteList::removeObserver(this);
	gAvatarTracker.removeObserver(this);
	gIdleCallbacks.deleteFunction(HBFloaterRadar::callbackIdle);
}

//virtual
bool HBFloaterRadar::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("tab_container");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("actions_tab");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("alerts_tab");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("moderation_tab");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("options_tab");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	// Avatar tab buttons:

	mProfileButton = getChild<LLButton>("profile_btn");
	mProfileButton->setClickedCallback(onClickProfile, this);

	mTrackButton = getChild<LLButton>("track_btn");
	mTrackButton->setClickedCallback(onClickTrack, this);

	mIMButton = getChild<LLButton>("im_btn");
	mIMButton->setClickedCallback(onClickIM, this);

	mTPOfferButton = getChild<LLButton>("offer_btn");
	mTPOfferButton->setClickedCallback(onClickTeleportOffer, this);

	mRequestTPButton = getChild<LLButton>("request_tp_btn");
	mRequestTPButton->setClickedCallback(onClickTeleportRequest, this);

	mTeleportToButton = getChild<LLButton>("teleport_btn");
	mTeleportToButton->setClickedCallback(onClickTeleportTo, this);

	mMarkButton = getChild<LLButton>("mark_btn");
	mMarkButton->setClickedCallback(onClickMark, this);

	mPrevMarkedButton = getChild<LLButton>("prev_marked_btn");
	mPrevMarkedButton->setClickedCallback(onClickPrevMarked, this);

	mNextMarkedButton = getChild<LLButton>("next_marked_btn");
	mNextMarkedButton->setClickedCallback(onClickNextMarked, this);

	mFocusButton = getChild<LLButton>("focus_btn");
	mFocusButton->setClickedCallback(onClickFocus, this);

	mPrevInListButton = getChild<LLButton>("prev_in_list_btn");
	mPrevInListButton->setClickedCallback(onClickPrevInList, this);

	mNextInListButton = getChild<LLButton>("next_in_list_btn");
	mNextInListButton->setClickedCallback(onClickNextInList, this);

	// Alerts tab check boxes:

	mRadarAlertsCheck = getChild<LLCheckBoxCtrl>("radar_alerts");
	mRadarAlertsCheck->setCommitCallback(onCheckRadarAlerts);
	mRadarAlertsCheck->setCallbackUserData(this);

	mSimAlertsCheck = getChild<LLCheckBoxCtrl>("alerts_sim");
	mDrawAlertsCheck = getChild<LLCheckBoxCtrl>("alerts_draw");
	mShoutAlertsCheck = getChild<LLCheckBoxCtrl>("alerts_shout");
	mChatAlertsCheck = getChild<LLCheckBoxCtrl>("alerts_chat");
	// Sync the check boxes enabled state already
	onCheckRadarAlerts(mRadarAlertsCheck, this);

	// Moderation tab buttons:

	mMuteButton = getChild<LLButton>("mute_btn");
	mMuteButton->setClickedCallback(onClickMute, this);

	mFreezeButton = getChild<LLButton>("freeze_btn");
	mFreezeButton->setClickedCallback(onClickFreeze, this);

	mARButton = getChild<LLButton>("ar_btn");
	mARButton->setClickedCallback(onClickAR, this);

	mEjectButton = getChild<LLButton>("eject_btn");
	mEjectButton->setClickedCallback(onClickEject, this);

	mEstateEjectButton = getChild<LLButton>("estate_eject_btn");
	mEstateEjectButton->setClickedCallback(onClickEjectFromEstate, this);

	mGetKeyButton = getChild<LLButton>("get_key_btn");
	mGetKeyButton->setClickedCallback(onClickGetKey, this);

	mDerenderButton = getChild<LLButton>("derender_btn");
	mDerenderButton->setClickedCallback(onClickDerender, this);

	mRerenderButton = getChild<LLButton>("rerender_btn");
	mRerenderButton->setClickedCallback(onClickDerender, this);

	// Options tab button:

	childSetAction("send_keys_btn", onClickSendKeys, this);

	mClearSavedMarkedButton = getChild<LLButton>("clear_marked_btn");
	mClearSavedMarkedButton->setClickedCallback(onClickClearSavedMarked, this);

	mUseLegacyNamesCheck = getChild<LLCheckBoxCtrl>("radar_use_legacy_names");
	mUseLegacyNamesCheck->setCommitCallback(onCheckUseLegacyNames);
	mUseLegacyNamesCheck->setCallbackUserData(this);

	// Scroll list
	mAvatarList = getChild<LLScrollListCtrl>("avatar_list");
	mAvatarList->sortByColumn("distance", true);
	mAvatarList->setCommitOnSelectionChange(true);
	mAvatarList->setCommitCallback(onSelectName);
	mAvatarList->setDoubleClickCallback(onDoubleClick);
	mAvatarList->setCallbackUserData(this);

	// Make sure the cached values will be properly updated on setting changes.
	connectRefreshCachedSettingsSafe("RadarMarkColor");
	connectRefreshCachedSettingsSafe("RadarNameColor");
	connectRefreshCachedSettingsSafe("RadarFriendNameColor");
	connectRefreshCachedSettingsSafe("RadarMutedNameColor");
	connectRefreshCachedSettingsSafe("RadarDerenderedNameColor");
	connectRefreshCachedSettingsSafe("RadarFarDistanceColor");
	connectRefreshCachedSettingsSafe("RadarShoutDistanceColor");
	connectRefreshCachedSettingsSafe("RadarChatDistanceColor");
	connectRefreshCachedSettingsSafe("RadarUpdatesPerSecond");
	connectRefreshCachedSettingsSafe("RadarRememberMarked");
	// Update cached setting values now.
	refreshCachedSettings();
	// Cache these UI strings once, i.e. for the session duration: this has
	// to be done here (and not in refreshCachedSettings() which is a static
	// method), so that we can extract the strings from 'this' floater XUI.
	if (sCardinals.empty())
	{
		// Cardinal points: a 4 characters string for North, South, West and
		// East, in this order. E.g. "NSWE". Note: only ASCII (one byte)
		// characters are accepted !
		sCardinals = getString("cardinals");
		if (sCardinals.size() != 4)
		{
			llwarns << "Invalid cardinals string in floater XUI definition."
					<< llendl;
			sCardinals = "NSWE";
		}
		// Strings used for the number of avatars in the list
		sTotalAvatarsStr = COMMENT_PREFIX + getString("total_avatars");
		sNoAvatarStr = COMMENT_PREFIX + getString("no_avatar");
		// Used for the tracker arrow text
		sLastKnownPosStr = "\n" + getString("last_known_pos");
		// Strings used for announcements
		sHasEnteredStr = getString("has_entered");
		sHasLeftStr = getString("has_left");
		sTheSimStr = getString("the_sim");
		sDrawDistanceStr = getString("draw_distance");
		sShoutRangeStr = getString("shout_range");
		sChatRangeStr = getString("chat_range");
	}

	// Load the marked avatars list
	loadMarkedFromFile();

	updateAvatarList();

	gIdleCallbacks.addFunction(HBFloaterRadar::callbackIdle);

	mTabContainer->selectTab(gSavedSettings.getS32("LastRadarTab"));

	return true;
}

//static
void HBFloaterRadar::refreshCachedSettings()
{
	// Note: do not bother using LLCachedControls here: this method is rarely
	// ever called.
	sMarkColor = gColors.getColor("RadarMarkColor");
	sNameColor = gColors.getColor("RadarNameColor");
	sFriendNameColor = gColors.getColor("RadarFriendNameColor");
	sMutedNameColor = gColors.getColor("RadarMutedNameColor");
	sDerenderedNameColor = gColors.getColor("RadarDerenderedNameColor");
	sFarDistanceColor = gColors.getColor("RadarFarDistanceColor");
	sShoutDistanceColor = gColors.getColor("RadarShoutDistanceColor");
	sChatDistanceColor = gColors.getColor("RadarChatDistanceColor");
	sUpdatesPerSecond = gSavedSettings.getU32("RadarUpdatesPerSecond");
	sRememberMarked = gSavedSettings.getBool("RadarRememberMarked");
}

//static
void HBFloaterRadar::connectRefreshCachedSettingsSafe(const char* name)
{
	LLControlVariable* controlp = gColors.getControl(name);
	if (!controlp)
	{
		controlp = gSavedSettings.getControl(name);
	}
	if (!controlp)
	{
		llwarns << "Setting name not found: " << name << llendl;
		return;
	}
	controlp->getSignal()->connect(boost::bind(&HBFloaterRadar::refreshCachedSettings));
}

//virtual
void HBFloaterRadar::onOpen()
{
	bool visible = true;
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames || gRLInterface.mContainsShowNearby ||
		 gRLInterface.mContainsShownametags))
	{
		// The floater will be automatically destroyed on the next idle
		// callback. Just make it invisible till then (better than destroying
		// the floater during an onOpen() event...).
		visible = false;
	}
//mk
	gSavedSettings.setBool("ShowRadar", visible);
	setVisible(visible);
}

//virtual
void HBFloaterRadar::onClose(bool app_quitting)
{
	setVisible(false);
	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowRadar", false);
	}
	if (!gSavedSettings.getBool("RadarKeepOpen") || app_quitting)
	{
		destroy();
	}
}

// Mute list observer
//virtual
void HBFloaterRadar::onChange()
{
	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; ++iter)
	{
		iter->second.mMuted = LLMuteList::isMuted(iter->first);
	}
}

// Friends list observer
//virtual
void HBFloaterRadar::changed(U32 mask)
{
	if (mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		for (avatar_list_t::iterator iter = mAvatars.begin(),
									 end = mAvatars.end();
			 iter != end; ++iter)
		{
			iter->second.mFriend = LLAvatarTracker::isAgentFriend(iter->first);
		}
	}
}

void HBFloaterRadar::updateAvatarList()
{
	LLViewerRegion* aregionp = gAgent.getRegion();
	if (!aregionp) return;

	U32 use_display_names = LLAvatarNameCache::useDisplayNames();
	static LLCachedControl<bool> legacy(gSavedSettings, "RadarUseLegacyNames");
	bool use_legacy_names = use_display_names == 0 || legacy;
	mUseLegacyNamesCheck->setEnabled(use_display_names != 0);
	mUseLegacyNamesCheck->set(use_legacy_names);

	LLVector3d mypos = gAgent.getPositionGlobal();
	F32 chat_range = (F32)aregionp->getChatRange();
	F32 shout_range = (F32)aregionp->getShoutRange();

	uuid_vec_t avatar_ids;
	std::vector<LLVector3d> positions;
	gWorld.getAvatars(avatar_ids, &positions, NULL, mypos, 65536.f);

	LLVector3d position;
	std::string name, display_name;
	for (S32 i = 0, count = avatar_ids.size(); i < count; ++i)
	{
		const LLUUID& avid = avatar_ids[i];
		if (avid.isNull())
		{
			continue;
		}

		LLVOAvatar* avatarp = gObjectList.findAvatar(avid);
		if (avatarp)
		{
			// Get avatar data
			position =
				gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());

			name = display_name = avatarp->getFullname();
			if (name.empty() &&
				(!gCacheNamep || !gCacheNamep->getFullName(avid, name)))
			{
				continue; // prevent (Loading...)
			}

			if (!use_legacy_names)
			{
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(avid, &avatar_name))
				{
					if (use_display_names == 2)
					{
						display_name = avatar_name.mDisplayName;
					}
					else
					{
						display_name = avatar_name.getNames();
					}
				}
			}

			avatar_list_t::iterator it = mAvatars.find(avid);
			if (it == mAvatars.end())
			{
				// Avatar not there yet, add it
				bool marked = sRememberMarked && mMarkedAvatars.count(avid);
				// C++11 is not the most limpid language... This whole
				// gymnastic with piecewise_construct and forward_as_tuple is
				// here to ensure HBRadarListEntry is constructed emplace, and
				// not just the UUID key of the map.
				mAvatars.emplace(std::piecewise_construct,
								 std::forward_as_tuple(avid),
								 std::forward_as_tuple(avatarp, avid ,name,
													   display_name, position,
													   marked));
			}
			else
			{
				HBRadarListEntry& entry = it->second;
				// Avatar already in list, update position
				F32 dist = (F32)(position - mypos).length();
				entry.setPosition(position, avatarp->getRegion() == aregionp,
								  true, dist < chat_range, dist < shout_range);
				// Update avatar display name.
				entry.setDisplayName(display_name);
			}
		}
		else
		{
			if (i >= (S32)positions.size())
			{
				continue;
			}
			position = positions[i];

			if (!gCacheNamep || !gCacheNamep->getFullName(avid, name))
			{
				continue; // Prevents (Loading...)
			}

			display_name = name;
			if (!use_legacy_names)
			{
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(avid, &avatar_name))
				{
					if (use_display_names == 2)
					{
						display_name = avatar_name.mDisplayName;
					}
					else
					{
						display_name = avatar_name.getNames();
					}
				}
			}

			avatar_list_t::iterator it = mAvatars.find(avid);
			if (it == mAvatars.end())
			{
				// Avatar not there yet, add it
				bool marked = sRememberMarked && mMarkedAvatars.count(avid);
				// C++11 is not the most limpid language... This whole
				// gymnastic with piecewise_construct and forward_as_tuple is
				// here to ensure HBRadarListEntry is constructed emplace, and
				// not just the UUID key of the map.
				mAvatars.emplace(std::piecewise_construct,
								 std::forward_as_tuple(avid),
								 std::forward_as_tuple(avatarp, avid ,name,
													   display_name, position,
													   marked));
			}
			else
			{
				HBRadarListEntry& entry = it->second;
				// Avatar already in list, update position
				F32 dist = (F32)(position - mypos).length();
				entry.setPosition(position,
								  aregionp->pointInRegionGlobal(position),
								  false, dist < chat_range,
								  dist < shout_range);
				// Update avatar display name.
				entry.setDisplayName(display_name);
			}
		}
	}

	expireAvatarList();
	refreshAvatarList();
	refreshTracker();
}

void HBFloaterRadar::expireAvatarList()
{
	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; )
	{
		avatar_list_t::iterator curiter = iter++;
		HBRadarListEntry& entry = curiter->second;
		if (!entry.getAlive() && entry.isDead())
		{
			LL_DEBUGS("Radar") << "Radar: expiring avatar "
							   << entry.getDisplayName() << LL_ENDL;
			if (curiter->first == mTrackedAvatar)
			{
				stopTracker();
			}
			mAvatars.erase(curiter);
		}
	}
}

// Redraws the avatar list
void HBFloaterRadar::refreshAvatarList()
{
	// Do not update the list when the floater is hidden
	if (!getVisible() || isMinimized()) return;

	// We rebuild the list fully each time it is refreshed. The assumption is
	// that it is faster than to refresh each entry and sort again the list.
	uuid_vec_t selected = mAvatarList->getSelectedIDs();
	S32 scrollpos = mAvatarList->getScrollPos();

	mAvatarList->deleteAllItems();

	LLViewerRegion* aregionp = gAgent.getRegion();
	if (!aregionp) return;
	F32 chat_range = (F32)aregionp->getChatRange();
	F32 shout_range = (F32)aregionp->getShoutRange();
	S32 sim_width = aregionp->getWidth();

	bool marked_avatars = false;
	LLVector3d mypos = gAgent.getPositionGlobal();
	LLVector3d posagent, position, delta;
	posagent.set(gAgent.getPositionAgent());
	LLVector3d simpos = mypos - posagent;
	char temp[32];
	S32 in_sim = 0;
	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; ++iter)
	{
		HBRadarListEntry& entry = iter->second;
		// Skip if avatar has not been around
		if (entry.isDead())
		{
			continue;
		}

		position = entry.getPosition();
		delta = position - mypos;
		F32 distance = (F32)delta.length();
		static LLCachedControl<U32> unknwown_av_alt(gSavedSettings,
													"UnknownAvatarAltitude");
		bool unknown_altitude = false;
		if (position.mdV[VZ] == (F32)unknwown_av_alt)
		{
			unknown_altitude = true;
			distance = 9000.f;
		}
		delta.mdV[2] = 0.f;
		F32 side_distance = (F32)delta.length();

		// *HACK: Workaround for an apparent bug: sometimes avatar entries get
		// stuck, and are registered by the client as perpetually moving in the
		// same direction. This makes sure they get removed from the visible
		// list eventually
		if (side_distance > 2048.f)
		{
			continue;
		}

		const LLUUID& avid = iter->first;

		LLSD element;
		element["id"] = avid;

		LLSD& mark_column = element["columns"][LIST_MARK];
		mark_column["column"] = "marked";
		mark_column["type"] = "text";
		if (entry.isMarked() || entry.isCustomMark())
		{
			mark_column["value"] = entry.getMarkChar();
			mark_column["color"] = entry.getMarkColor().getValue();
			mark_column["font-style"] = "BOLD";
			if (entry.isMarked())
			{
				marked_avatars = true;
			}
		}
		else
		{
			mark_column["value"] = "";
		}

		LLSD& name_column = element["columns"][LIST_AVATAR_NAME];
		name_column["column"] = "avatar_name";
		name_column["type"] = "text";
		name_column["value"] = entry.getDisplayName().c_str();
		if (entry.getEntryAgeSeconds() > 1.f)
		{
			name_column["font-style"] = "ITALIC";
		}
		else if (entry.isFocused())
		{
			name_column["font-style"] = "BOLD";
		}
		if (entry.isDerendered())
		{
			name_column["color"] = sDerenderedNameColor.getValue();
		}
		else if (entry.isMuted())
		{
			name_column["color"] = sMutedNameColor.getValue();
		}
		else if (entry.isFriend())
		{
			name_column["color"] = sFriendNameColor.getValue();
		}
		else
		{
			name_column["color"] = entry.getColor().getValue();
		}

		LLColor4* color = &sNameColor;
		LLSD& dist_column = element["columns"][LIST_DISTANCE];
		dist_column["column"] = "distance";
		dist_column["type"] = "text";
		if (unknown_altitude)
		{
			strcpy(temp, "?");
			if (entry.isDrawn())
			{
				color = &sFarDistanceColor;
			}
		}
		else if (distance < shout_range)
		{
			snprintf(temp, sizeof(temp), "%.1f", distance);
			if (distance > chat_range)
			{
				color = &sShoutDistanceColor;
			}
			else
			{
				color = &sChatDistanceColor;
			}
		}
		else
		{
			if (entry.isDrawn())
			{
				color = &sFarDistanceColor;
			}
			snprintf(temp, sizeof(temp), "%d", (S32)distance);
		}
		dist_column["value"] = temp;
		dist_column["color"] = color->getValue();

		position = position - simpos;

		S32 x = (S32)position.mdV[VX];
		S32 y = (S32)position.mdV[VY];
		if (x >= 0 && x <= sim_width && y >= 0 && y <= sim_width)
		{
			snprintf(temp, sizeof(temp), "%d, %d", x, y);
			++in_sim;
		}
		else
		{
			temp[0] = '\0';
			if (y < 0)
			{
				strncat(temp, &sCardinals[1], 1);	// South
			}
			else if (y > sim_width)
			{
				strncat(temp, &sCardinals[0], 1);	// North
			}
			if (x < 0)
			{
				strncat(temp, &sCardinals[2], 1);	// West
			}
			else if (x > sim_width)
			{
				strncat(temp, &sCardinals[3], 1);	// East
			}
		}
		LLSD& pos_column = element["columns"][LIST_POSITION];
		pos_column["column"] = "position";
		pos_column["type"] = "text";
		pos_column["value"] = temp;

		LLSD& alt_column = element["columns"][LIST_ALTITUDE];
		alt_column["column"] = "altitude";
		alt_column["type"] = "text";
		if (unknown_altitude)
		{
			strcpy(temp, "?");
		}
		else
		{
			snprintf(temp, sizeof(temp), "%d", (S32)position.mdV[VZ]);
		}
		alt_column["value"] = temp;

		// Add to list
		LLScrollListItem* itemp = mAvatarList->addElement(element, ADD_BOTTOM);
		if (itemp)
		{
			itemp->setToolTip(entry.getToolTip());
		}
	}

	// Sort
	mAvatarList->sortItems();

	// Add the number of avatars as a comment at the bottom of the list
	S32 count = mAvatarList->getItemCount();
	std::string comment;
	if (count > 0)
	{
		comment = llformat(sTotalAvatarsStr.c_str(), count, in_sim);
	}
	else
	{
		comment = sNoAvatarStr;
	}
	mAvatarList->addCommentText(comment);

	// Finish
	mAvatarList->selectMultiple(selected);
	mAvatarList->setScrollPos(scrollpos);

	// Refresh the buttons
	mPrevMarkedButton->setEnabled(marked_avatars);
	mNextMarkedButton->setEnabled(marked_avatars);
	onSelectName(NULL, this); // NULL is used to flag this false commit event.
}

bool HBFloaterRadar::loadMarkedFromFile()
{
	mMarkedAvatars.clear();
	if (!sRememberMarked)
	{
		return true;
	}

	std::string file = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
													  "marked_avatars.lst");
	if (file.empty())
	{
		llwarns << "Marked avatars filename is empty !" << llendl;
		return false;
	}

	LLFILE* fp = LLFile::open(file, "rb");
	if (!fp)
	{
		llwarns << "Could not open marked avatars file " << file << llendl;
		return false;
	}

	char buffer[64], id_buffer[64];
	id_buffer[63] = '\0';
	while (!feof(fp) && fgets(buffer, 64, fp))
	{
		id_buffer[0] = '\0';
		if (sscanf(buffer, "%37s\n", id_buffer) == 1)
		{
			LLUUID id = LLUUID(id_buffer);
			if (id.notNull())
			{
				LL_DEBUGS("Radar") << "Adding UUID: " << id << LL_ENDL;
				mMarkedAvatars.emplace(id);
			}
		}
		else
		{
			llwarns_sparse << "Malformed marked avatars file !" << llendl;
		}
	}

	LLFile::close(fp);
	return true;
}

bool HBFloaterRadar::saveMarkedToFile(bool force)
{
	if (!force && !sRememberMarked)
	{
		return true;
	}

	std::string file = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
													  "marked_avatars.lst");
	if (file.empty())
	{
		llwarns << "Marked avatars filename is empty !" << llendl;
		return false;
	}

	LLFILE* fp = LLFile::open(file, "wb");
	if (!fp)
	{
		llwarns << "Could not open marked avatars file " << file << llendl;
		return false;
	}

	if (mMarkedAvatars.empty())
	{
		// Write a null UUID to ensure the old file is overwritten
		fprintf(fp, "%s\n", LLUUID::null.asString().c_str());
	}
	else
	{
		for (uuid_list_t::iterator it = mMarkedAvatars.begin(),
								   end = mMarkedAvatars.end();
			 it != end; ++it)
		{
			const LLUUID& id = *it;
			LL_DEBUGS("Radar") << "Saving UUID: " << id << LL_ENDL;
			fprintf(fp, "%s\n", id.asString().c_str());
		}
	}

	LLFile::close(fp);
	return true;
}

bool HBFloaterRadar::startTracker(const LLUUID& avid)
{
	avatar_list_t::iterator it = mAvatars.find(avid);
	if (it == mAvatars.end())
	{
		return false;
	}

	mTracking = true;
	mTrackedAvatar = avid;

	HBRadarListEntry& entry = it->second;
	std::string name = entry.getDisplayName();
	if (!sUpdatesPerSecond)
	{
		name += sLastKnownPosStr;
	}
	// Note: gTracker.trackAvatar() only works for friends allowing you to see
	// them on map, so we must use out own tracking code, with a position
	// tracker beacon instead.
	gTracker.trackLocation(entry.getPosition(), name, "");

	if (gAutomationp)
	{
		gAutomationp->onRadarTrack(avid, entry.getName(), true);
	}

	return true;
}

void HBFloaterRadar::stopTracker()
{
	if (mTracking && gAutomationp)
	{
		std::string name;
		avatar_list_t::iterator it = mAvatars.find(mTrackedAvatar);
		if (it != mAvatars.end())
		{
			HBRadarListEntry& entry = it->second;
			name = entry.getName();
		}
		gAutomationp->onRadarTrack(mTrackedAvatar, name, false);
	}

	gTracker.stopTracking();
	mTracking = false;
}

void HBFloaterRadar::refreshTracker()
{
	if (!mTracking) return;

	if (gTracker.isTracking())
	{
		avatar_list_t::iterator it = mAvatars.find(mTrackedAvatar);
		if (it == mAvatars.end())
		{
			stopTracker();
			return;
		}
		HBRadarListEntry& entry = it->second;

		LLVector3d pos;
		if (sUpdatesPerSecond)
		{
			pos = entry.getPosition();
		}
		else
		{
			LLVOAvatar* avatarp = gObjectList.findAvatar(mTrackedAvatar);
			if (!avatarp)
			{
				stopTracker();
				return;
			}
			pos = gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());
		}

		F32 dist = (pos - gTracker.getTrackedPositionGlobal()).length();
		if (dist > 1.f)
		{
			std::string name = entry.getDisplayName();
			gTracker.trackLocation(pos, name, "");
		}
	}
	else
	{
		stopTracker();
	}
}

HBRadarListEntry* HBFloaterRadar::getAvatarEntry(const LLUUID& avid)
{
	if (avid.isNull())
	{
		return NULL;
	}

	avatar_list_t::iterator iter = mAvatars.find(avid);
	return iter != mAvatars.end() ? &iter->second : NULL;
}

void HBFloaterRadar::removeFocusFromAll()
{
	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; ++iter)
	{
		HBRadarListEntry* entry = &iter->second;
		entry->setFocus(false);
	}
}

void HBFloaterRadar::focusOnCurrent()
{
	if (mAvatars.empty())
	{
		return;
	}

	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; iter++)
	{
		if (iter->first == mFocusedAvatar)
		{
			HBRadarListEntry& entry = iter->second;
			if (!entry.isDead())
			{
				removeFocusFromAll();
				entry.setFocus(true);
				gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
			}
			break;
		}
	}
}

void HBFloaterRadar::focusOnPrev(bool marked_only)
{
	if (mAvatars.empty())
	{
		return;
	}

	HBRadarListEntry* prev = NULL;

	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; ++iter)
	{
		HBRadarListEntry* entry = &iter->second;
		if (entry->isDead())
		{
			continue;
		}

		if (prev && iter->first == mFocusedAvatar)
		{
			break;
		}

		if ((!marked_only && entry->isDrawn()) || entry->isMarked())
		{
			prev = entry;
		}
	}

	if (prev)
	{
		removeFocusFromAll();
		prev->setFocus(true);
		mFocusedAvatar = prev->getID();
		mAvatarList->selectByID(mFocusedAvatar);
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

void HBFloaterRadar::focusOnNext(bool marked_only)
{
	if (mAvatars.empty())
	{
		return;
	}

	HBRadarListEntry* next = NULL;

	bool found = false;
	for (avatar_list_t::iterator iter = mAvatars.begin(), end = mAvatars.end();
		 iter != end; ++iter)
	{
		HBRadarListEntry* entry = &iter->second;
		if (entry->isDead())
		{
			continue;
		}

		if (!next && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
		}

		if (found && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
			break;
		}

		if (iter->first == mFocusedAvatar)
		{
			found = true;
		}
	}

	if (next)
	{
		removeFocusFromAll();
		next->setFocus(true);
		mFocusedAvatar = next->getID();
		mAvatarList->selectByID(mFocusedAvatar);
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

void HBFloaterRadar::doCommand(void (*func)(const LLUUID& avid))
{
	uuid_vec_t ids = mAvatarList->getSelectedIDs();
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& avid = ids[i];
		HBRadarListEntry* entry = getAvatarEntry(avid);
		if (entry)
		{
			llinfos << "Executing command on " << entry->getDisplayName()
					<< llendl;
			func(avid);
		}
	}
}

std::string HBFloaterRadar::getSelectedNames(const std::string& separator)
{
	std::string ret;

	uuid_vec_t ids = mAvatarList->getSelectedIDs();
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& avid = ids[i];
		HBRadarListEntry* entry = getAvatarEntry(avid);
		if (entry)
		{
			if (!ret.empty())
			{
				ret += separator;
			}
			ret += entry->getName();
		}
	}

	return ret;
}

std::string HBFloaterRadar::getSelectedName()
{
	LLUUID id = getSelectedID();
	HBRadarListEntry* entry = getAvatarEntry(id);
	return entry ? entry->getName() : "";
}

LLUUID HBFloaterRadar::getSelectedID()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	return item ? item->getUUID() : LLUUID::null;
}

//static
bool HBFloaterRadar::setAvatarNameColor(const LLUUID& id, const LLColor4& col)
{
	HBFloaterRadar* self = findInstance();
	if (!self)
	{
		return true;	// When no radar instance exists, report a success.
	}

	// First, make sure the list is up to date.
	self->updateAvatarList();

	HBRadarListEntry* entry = self->getAvatarEntry(id);
	if (!entry)
	{
		return false;	// Avatar not found in Radar list.
	}

	entry->setColor(col);
	return true;
}

//static
void HBFloaterRadar::setRenderStatusDirty(const LLUUID& avid)
{
	HBFloaterRadar* self = findInstance();
	if (!self)
	{
		return;	// Nothing to do.
	}

	bool maybe_derendered = !LLViewerObjectList::sBlackListedObjects.empty();

	if (avid.notNull())
	{
		HBRadarListEntry* entry = self->getAvatarEntry(avid);
		if (entry)
		{
			entry->mDerendered =
				maybe_derendered &&
				LLViewerObjectList::sBlackListedObjects.count(avid);
		}
		return;
	}

	for (avatar_list_t::iterator it = self->mAvatars.begin(),
								 end = self->mAvatars.end();
		 it != end; ++it)
	{
		it->second.mDerendered =
			maybe_derendered &&
			LLViewerObjectList::sBlackListedObjects.count(it->first);
	}
}

//static
void HBFloaterRadar::onTabChanged(void* userdata, bool from_click)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastRadarTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void HBFloaterRadar::onClickIM(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
		LLAvatarActions::startIM(ids);
	}
}

//static
void HBFloaterRadar::onClickTeleportOffer(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
		if (!ids.empty())
		{
			LLAvatarActions::offerTeleport(ids);
		}
	}
}

//static
void HBFloaterRadar::onClickTeleportRequest(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
		if (!ids.empty())
		{
			LLAvatarActions::teleportRequest(ids[0]);
		}
	}
}

//static
void HBFloaterRadar::onClickTrack(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mAvatarList->getFirstSelected();
	if (!item) return;

	LLUUID avid = item->getUUID();
	if (self->mTracking && self->mTrackedAvatar == avid)
	{
		self->stopTracker();
	}
	else
	{
		self->startTracker(avid);
	}
}

//static
void HBFloaterRadar::onClickMark(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& avid = ids[i];
		HBRadarListEntry* entry = self->getAvatarEntry(avid);
		if (entry)
		{
			bool marked = entry->toggleMark();
			if (marked)
			{
				self->mMarkedAvatars.emplace(avid);
			}
			else
			{
				self->mMarkedAvatars.erase(avid);
			}
			if (gAutomationp)
			{
				gAutomationp->onRadarMark(avid, entry->getName(), marked);
			}
		}
	}

	self->saveMarkedToFile();
}

//static
void HBFloaterRadar::onClickFocus(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		self->mFocusedAvatar = item->getUUID();
		self->focusOnCurrent();
	}
}

//static
void HBFloaterRadar::onClickPrevInList(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->focusOnPrev(false);
	}
}

//static
void HBFloaterRadar::onClickNextInList(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->focusOnNext(false);
	}
}

//static
void HBFloaterRadar::onClickPrevMarked(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->focusOnPrev(true);
	}
}

//static
void HBFloaterRadar::onClickNextMarked(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->focusOnNext(true);
	}
}

//static
void HBFloaterRadar::onClickClearSavedMarked(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->mMarkedAvatars.clear();

		if (sRememberMarked && self->mAvatars.size() > 0)
		{
			for (avatar_list_t::iterator iter = self->mAvatars.begin(),
										 end = self->mAvatars.end();
				 iter != end; ++iter)
			{
				if (iter->second.isMarked())
				{
					self->mMarkedAvatars.emplace(iter->first);
				}
			}
		}

		self->saveMarkedToFile(true);
	}
}

//static
void HBFloaterRadar::onCheckRadarAlerts(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enabled = check->get();
		self->mSimAlertsCheck->setEnabled(enabled);
		self->mDrawAlertsCheck->setEnabled(enabled);
		self->mShoutAlertsCheck->setEnabled(enabled);
		self->mChatAlertsCheck->setEnabled(enabled);
	}
}

//static
void HBFloaterRadar::onCheckUseLegacyNames(LLUICtrl* ctrl, void*)
{
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (check)
	{
		gSavedSettings.setBool("RadarUseLegacyNames", check->get());
	}
}

//static
void HBFloaterRadar::onClickGetKey(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mAvatarList->getFirstSelected();
	if (!item) return;

	LLUUID avid = item->getUUID();
	if (gWindowp)
	{
		gWindowp->copyTextToClipboard(utf8str_to_wstring(avid.asString()));
	}
}

//static
void HBFloaterRadar::onClickSendKeys(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self || self->mAvatars.empty())
	{
		return;
	}

	for (avatar_list_t::iterator iter = self->mAvatars.begin(),
								 end = self->mAvatars.end();
		 iter != end; ++iter)
	{
		const HBRadarListEntry& entry = iter->second;
		if (!entry.isDead() && entry.isInSim())
		{
			announce(iter->first.asString());
		}
	}
}

static void send_estate_message(const char* request, const LLUUID& target)
{
	if (!gAgent.getRegion()) return;

	LLUUID invoice;
	// This seems to provide an ID so that the sim can say which request it's
	// replying to.
	invoice.generate();

	llinfos << "Sending estate request '" << request << "'" << llendl;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_EstateOwnerMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock(_PREHASH_MethodData);
	msg->addString(_PREHASH_Method, request);
	msg->addUUID(_PREHASH_Invoice, invoice);

	// Agent id
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, gAgentID.asString().c_str());

	// Target
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, target.asString().c_str());

	msg->sendReliable(gAgent.getRegionHost());
}

static void cmd_freeze(const LLUUID& avid)
{
	LLAvatarActions::sendFreeze(avid, true);
}

static void cmd_unfreeze(const LLUUID& avid)
{
	LLAvatarActions::sendFreeze(avid, false);
}

static void cmd_eject(const LLUUID& avid)
{
	LLAvatarActions::sendEject(avid, false);
}

static void cmd_ban(const LLUUID& avid)
{
	LLAvatarActions::sendEject(avid, true);
}

static void cmd_profile(const LLUUID& avid)
{
	LLFloaterAvatarInfo::showFromDirectory(avid);
}

static void cmd_estate_eject(const LLUUID& avid)
{
	send_estate_message("teleporthomeuser", avid);
}

//static
void HBFloaterRadar::callbackFreeze(const LLSD& notification,
									const LLSD& response)
{
	HBFloaterRadar* self = findInstance();
	if (!self) return;

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		self->doCommand(cmd_freeze);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_unfreeze);
	}
}

//static
void HBFloaterRadar::callbackEject(const LLSD& notification,
								   const LLSD& response)
{
	HBFloaterRadar* self = findInstance();
	if (!self) return;

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		self->doCommand(cmd_eject);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_ban);
	}
}

//static
void HBFloaterRadar::callbackEjectFromEstate(const LLSD& notification,
											 const LLSD& response)
{
	HBFloaterRadar* self = findInstance();
	if (!self) return;

	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		self->doCommand(cmd_estate_eject);
	}
}

//static
void HBFloaterRadar::callbackIdle(void* userdata)
{
	static U32 last_update_frame = 0;

	LL_FAST_TIMER(FTM_IDLE_CB_RADAR);

	HBFloaterRadar* self = findInstance();
	if (!self) return;

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames || gRLInterface.mContainsShowNearby ||
		 gRLInterface.mContainsShownametags))
	{
		self->destroy();
		gSavedSettings.setBool("ShowRadar", false);
		return;
	}
//mk

	// In case of slow rendering do not cause more lag...
	if (gFrameCount - last_update_frame > 4)
	{
		if (sUpdatesPerSecond)
		{
			if (sUpdateTimer.getElapsedTimeF32() >=
					1.f / (F32)sUpdatesPerSecond)
			{
				self->updateAvatarList();
				sUpdateTimer.reset();
				last_update_frame = gFrameCount;
			}
		}
		else
		{
			self->refreshTracker();
		}
	}
}

//static
void HBFloaterRadar::onClickFreeze(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	LLSD args;
	args["AVATAR_NAME"] = self->getSelectedNames();
	gNotifications.add("FreezeAvatarFullname", args, LLSD(), callbackFreeze);
}

//static
void HBFloaterRadar::onClickEject(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	LLSD args;
	args["AVATAR_NAME"] = self->getSelectedNames();
	gNotifications.add("EjectAvatarFullname", args, LLSD(), callbackEject);
}

//static
void HBFloaterRadar::onClickMute(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& avid = ids[i];
		avatar_list_t::iterator it = self->mAvatars.find(avid);
		if (it == self->mAvatars.end())
		{
			llwarns << "Could not find " << avid
					<< " in the Radar list; (un)mute action aborted."
					<< llendl;
			continue;
		}

		const std::string& name = it->second.getName();
		if (LLMuteList::isMuted(avid))
		{
			LLMute mute(avid, name, LLMute::AGENT);
			LLMuteList::remove(mute);
		}
		else
		{
			LLMute mute(avid, name, LLMute::AGENT);
			if (LLMuteList::add(mute))
			{
				LLFloaterMute::selectMute(mute.mID);
			}
		}
	}
}

//static
void HBFloaterRadar::onClickDerender(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& avid = ids[i];

		bool derendered = LLViewerObjectList::sBlackListedObjects.count(avid);
		if (derendered)
		{
			// Remove from the black list
			LLViewerObjectList::sBlackListedObjects.erase(avid);
		}
		else
		{
			// Add to the black list
			LLViewerObjectList::sBlackListedObjects.emplace(avid);

			// Derender by killing the object.
			LLViewerObject* vobj = gObjectList.findObject(avid);
			if (vobj)
			{
				gObjectList.killObject(vobj);
			}
		}

		// Update any cached derendered status
		HBRadarListEntry* entry = self->getAvatarEntry(avid);
		if (entry)
		{
			entry->mDerendered = !derendered; // Status just got toggled
		}
	}
}

//static
void HBFloaterRadar::onClickEjectFromEstate(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	LLSD args;
	LLSD payload;
	args["EVIL_USER"] = self->getSelectedNames();
	gNotifications.add("EstateKickUser", args, payload,
					   callbackEjectFromEstate);
}

//static
void HBFloaterRadar::onClickAR(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID avid = item->getUUID();
		if (self->getAvatarEntry(avid))
		{
			LLFloaterReporter::showFromAvatar(avid);
		}
	}
}

//static
void HBFloaterRadar::onClickProfile(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
		self->doCommand(cmd_profile);
	}
}

//static
void HBFloaterRadar::onClickTeleportTo(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID avid = item->getUUID();
		HBRadarListEntry* entry = self->getAvatarEntry(avid);
		if (entry)
		{
			gAgent.teleportViaLocation(entry->getPosition());
		}
	}
}

//static
void HBFloaterRadar::onDoubleClick(void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (self)
	{
 		LLScrollListItem* item = self->mAvatarList->getFirstSelected();
		if (item)
		{
			const LLUUID& avid = item->getUUID();
			if (gObjectList.findAvatar(avid))
			{
				HBFloaterInspectAvatar::show(avid);
			}
		}
	}
}

//static
void HBFloaterRadar::onSelectName(LLUICtrl* ctrlp, void* userdata)
{
	HBFloaterRadar* self = (HBFloaterRadar*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mAvatarList->getSelectedIDs();
	U32 count = ids.size();

	// Check whether selected avatars are in the same state or not regarding
	// marking, muting and rendering, so that the corresponding button is only
	// enabled when the same action can be performed on them.
	bool marked = false;
	bool unmarked = false;
	bool muted = false;
	bool unmuted = false;
	bool derendered = false;
	bool rendered = false;
	for (U32 i = 0; i < count; ++i)
	{
		const LLUUID& avid = ids[i];
		HBRadarListEntry* entry = self->getAvatarEntry(avid);
		if (!entry)
		{
			continue;
		}
		if (entry->isMarked())
		{
			marked = true;
		}
		else
		{
			unmarked = true;
		}
		if (entry->isMuted())
		{
			muted = true;
		}
		else
		{
			unmuted = true;
		}
		if (entry->isDerendered())
		{
			derendered = true;
		}
		else
		{
			rendered = true;
		}
	}
	self->mMarkButton->setEnabled(marked != unmarked);
	self->mMuteButton->setEnabled(muted != unmuted);
	self->mDerenderButton->setEnabled(!derendered && rendered);
	self->mRerenderButton->setEnabled(derendered && !rendered);

	// Buttons that must be enabled when one or more avatars are selected
	bool enabled = count > 0;
	self->mProfileButton->setEnabled(enabled);
	self->mIMButton->setEnabled(enabled);
	self->mTPOfferButton->setEnabled(enabled);
	self->mFreezeButton->setEnabled(enabled);
	self->mEjectButton->setEnabled(enabled);
	self->mEstateEjectButton->setEnabled(enabled);

	// Buttons that must be enabled when only one avatar is selected
	enabled = count == 1;
	self->mTrackButton->setEnabled(enabled);
	self->mRequestTPButton->setEnabled(enabled);
	self->mTeleportToButton->setEnabled(enabled);
	self->mRequestTPButton->setEnabled(enabled);
	self->mARButton->setEnabled(enabled);
	self->mGetKeyButton->setEnabled(enabled);

	// Buttons that must be enabled when the selected avatar is drawn
	if (enabled)
	{
		enabled = false;
		LLScrollListItem* item = self->mAvatarList->getFirstSelected();
		if (item)
		{
			LLUUID avid = item->getUUID();
			HBRadarListEntry* entry = self->getAvatarEntry(avid);
			if (entry)
			{
				enabled = entry->isDrawn();
			}
		}
		self->mFocusButton->setEnabled(enabled);
		self->mPrevInListButton->setEnabled(enabled);
		self->mNextInListButton->setEnabled(enabled);
	}

	// Note: ctrlp is NULL when this method gets called after a list refresh
	// and we do not want this false commit event transmitted to the Lua
	// callback. Likewise, a commit happens when the list is emptied, and we
	// do not want this event to be transmitted, thus the test for empty ids.
	// Finally, we only transmit the selection when it changed.
	if (gAutomationp && ctrlp && !ids.empty() && self->mLastSelection != ids)
	{
		gAutomationp->onRadarSelection(ids);
		self->mLastSelection.swap(ids);
	}
}
