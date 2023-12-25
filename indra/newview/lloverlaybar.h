/**
 * @file lloverlaybar.h
 * @brief LLOverlayBar class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLOVERLAYBAR_H
#define LL_LLOVERLAYBAR_H

#include "boost/signals2.hpp"

#include "llcontrol.h"
#include "llframetimer.h"
#include "llpanel.h"

#include "llpathfindingmanager.h"

constexpr S32 OVERLAY_BAR_HEIGHT = 20;

class LLButton;
class LLMediaRemoteCtrl;
class LLVoiceRemoteCtrl;

class LLOverlayBar final : public LLPanel
{
protected:
	LOG_CLASS(LLOverlayBar);

private:
	// Navmesh rebaking stuff
	typedef enum
	{
		kRebakeNavMesh_Available,
		kRebakeNavMesh_RequestSent,
		kRebakeNavMesh_InProgress,
		kRebakeNavMesh_NotAvailable,
		kRebakeNavMesh_Default = kRebakeNavMesh_NotAvailable
	} ERebakeNavMeshMode;

public:
	LLOverlayBar(const LLRect& rect);
	~LLOverlayBar() override;

	void refresh() override;
	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void setVisible(bool visible) override;

	LL_INLINE void setDirty()							{ mDirty = true; }

	// Callback functions used by llmediaremotectrl.cpp:
	static void toggleAudioVolumeFloater(void*);

	// Navmesh rebaking stuff (used by llstatusbar.cpp and
	// hbviewerautomation.cpp).

	LL_INLINE bool isNavmeshDirty() const
	{
		return mRebakeNavMeshMode == kRebakeNavMesh_Available;
	}

	LL_INLINE bool isNavmeshRebaking() const
	{
		return mRebakeNavMeshMode == kRebakeNavMesh_RequestSent ||
			   mRebakeNavMeshMode == kRebakeNavMesh_InProgress;
	}

	LL_INLINE bool canRebakeRegion() const			{ return mCanRebakeRegion; }

	// Lua status bar icon action
 	void setLuaFunctionButton(const std::string& label,
							  const std::string& command,
							  const std::string& tooltip);

private:
	void layoutButtons();

	// Navmesh rebaking stuff
	void setRebakeMode(ERebakeNavMeshMode mode);
	void handleAgentState(bool can_rebake_region);
	void handleRebakeNavMeshResponse(bool status_response);
	void handleNavMeshStatus(const LLPathfindingNavMeshStatus& statusp);
	void handleRegionBoundaryCrossed();
	void createNavMeshStatusListenerForCurrentRegion();

	static void* createMasterRemote(void* userdata);
	static void* createParcelMusicRemote(void* userdata);
	static void* createParcelMediaRemote(void* userdata);
	static void* createSharedMediaRemote(void* userdata);
	static void* createVoiceRemote(void* userdata);

	static void onClickIMReceived(void* data);
	static void onClickSetNotBusy(void* data);
	static void onClickPublicBaking(void* data);
	static void onClickMouselook(void* data);
	static void onClickStandUp(void* data);
	static void onClickResetView(void* data);
 	static void onClickFlycam(void* data);
 	static void onClickRebakeRegion(void* data);
 	static void onClickLuaFunction(void* data);

private:
	LLVoiceRemoteCtrl*		mVoiceRemote;
	LLMediaRemoteCtrl*		mSharedMediaRemote;
	LLMediaRemoteCtrl*		mParcelMediaRemote;
	LLMediaRemoteCtrl*		mParcelMusicRemote;
	LLMediaRemoteCtrl*		mMasterRemote;

	LLButton*				mBtnIMReceiced;
	LLButton*				mBtnSetNotBusy;
	LLButton*				mBtnFlyCam;
	LLButton*				mBtnMouseLook;
	LLButton*				mBtnStandUp;
	LLButton*				mBtnPublicBaking;
	LLButton*				mBtnRebakeRegion;
	LLButton*				mBtnLuaFunction;

	LLCachedControl<S32>	mStatusBarPad;

	S32						mVoiceRemoteWidth;
	S32						mParcelMediaRemoteWidth;
	S32						mSharedMediaRemoteWidth;
	S32						mParcelMusicRemoteWidth;
	S32						mMasterRemoteWidth;

	U32						mLastIMsCount;
	std::string				mIMReceivedlabel;

	std::string				mLuaCommand;

	LLFrameTimer			mUpdateTimer;

	// Navmesh rebaking stuff
	ERebakeNavMeshMode							mRebakeNavMeshMode;
	LLPathfindingNavMesh::navmesh_slot_t		mNavMeshSlot;
	boost::signals2::connection					mRegionCrossingSlot;
	LLPathfindingManager::agent_state_slot_t	mAgentStateSlot;
	LLUUID										mRebakingNotificationID;
	bool										mCanRebakeRegion;

	bool					mBuilt;
	bool					mDirty;
};

extern LLOverlayBar* gOverlayBarp;

#endif
