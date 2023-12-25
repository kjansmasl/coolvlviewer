/**
 * @file llagent.h
 * @brief LLAgent class header file
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLAGENT_H
#define LL_LLAGENT_H

#include <set>
#include <vector>

#include "boost/signals2.hpp"

#include "indra_constants.h"
#include "llavatarappearancedefines.h"
#include "llcharacter.h"
#include "llcontrol.h"
#include "llcoordframe.h"
#include "llcorehttprequest.h"
#include "llcorehttputil.h"
#include "lldbstrings.h"
#include "llevent.h"
#include "llinventory.h"
#include "llmath.h"
#include "llpermissionsflags.h"
#include "llpointer.h"
#include "llquaternion.h"
#include "llrefcount.h"
#include "llstring.h"
#include "lltimer.h"
#include "lluuid.h"
#include "llmatrix3.h"
#include "llmatrix4.h"
#include "llvector3d.h"
#include "llcolor4.h"
#include "llvector4.h"

#include "llfollowcam.h"
#include "llhudeffectlookat.h"
#include "llhudeffectpointat.h"
#include "llviewerinventory.h"
#include "llviewerwearable.h"

// Typing indication
constexpr U8 AGENT_STATE_TYPING = 0x04;
// Set when agent has objects selected
constexpr U8 AGENT_STATE_EDITING = 0x10;

///////////////////////////////////////////////////////////////////////////////
// These constants used to be defined in a separate llagentconstants.h file
// that was only included by llagent.h, so it is best kept in the latter. HB
///////////////////////////////////////////////////////////////////////////////

constexpr U32 CONTROL_AT_POS_INDEX			= 0;
constexpr U32 CONTROL_AT_NEG_INDEX			= 1;
constexpr U32 CONTROL_LEFT_POS_INDEX		= 2;
constexpr U32 CONTROL_LEFT_NEG_INDEX		= 3;
constexpr U32 CONTROL_UP_POS_INDEX			= 4;
constexpr U32 CONTROL_UP_NEG_INDEX			= 5;
constexpr U32 CONTROL_PITCH_POS_INDEX		= 6;
constexpr U32 CONTROL_PITCH_NEG_INDEX		= 7;
constexpr U32 CONTROL_YAW_POS_INDEX			= 8;
constexpr U32 CONTROL_YAW_NEG_INDEX			= 9;
constexpr U32 CONTROL_FAST_AT_INDEX			= 10;
constexpr U32 CONTROL_FAST_LEFT_INDEX		= 11;
constexpr U32 CONTROL_FAST_UP_INDEX			= 12;
constexpr U32 CONTROL_FLY_INDEX				= 13;
constexpr U32 CONTROL_STOP_INDEX			= 14;
constexpr U32 CONTROL_FINISH_ANIM_INDEX		= 15;
constexpr U32 CONTROL_STAND_UP_INDEX		= 16;
constexpr U32 CONTROL_SIT_ON_GROUND_INDEX	= 17;
constexpr U32 CONTROL_MOUSELOOK_INDEX		= 18;
constexpr U32 CONTROL_NUDGE_AT_POS_INDEX	= 19;
constexpr U32 CONTROL_NUDGE_AT_NEG_INDEX	= 20;
constexpr U32 CONTROL_NUDGE_LEFT_POS_INDEX	= 21;
constexpr U32 CONTROL_NUDGE_LEFT_NEG_INDEX	= 22;
constexpr U32 CONTROL_NUDGE_UP_POS_INDEX	= 23;
constexpr U32 CONTROL_NUDGE_UP_NEG_INDEX	= 24;
constexpr U32 CONTROL_TURN_LEFT_INDEX		= 25;
constexpr U32 CONTROL_TURN_RIGHT_INDEX		= 26;
constexpr U32 CONTROL_AWAY_INDEX			= 27;
constexpr U32 CONTROL_LBUTTON_DOWN_INDEX	= 28;
constexpr U32 CONTROL_LBUTTON_UP_INDEX		= 29;
constexpr U32 CONTROL_ML_LBUTTON_DOWN_INDEX	= 30;
constexpr U32 CONTROL_ML_LBUTTON_UP_INDEX	= 31;
constexpr U32 TOTAL_CONTROLS				= 32;

constexpr U32 AGENT_CONTROL_AT_POS			= 0x1 << CONTROL_AT_POS_INDEX;			// 0x00000001
constexpr U32 AGENT_CONTROL_AT_NEG			= 0x1 << CONTROL_AT_NEG_INDEX;			// 0x00000002
constexpr U32 AGENT_CONTROL_LEFT_POS		= 0x1 << CONTROL_LEFT_POS_INDEX;		// 0x00000004
constexpr U32 AGENT_CONTROL_LEFT_NEG		= 0x1 << CONTROL_LEFT_NEG_INDEX;		// 0x00000008
constexpr U32 AGENT_CONTROL_UP_POS			= 0x1 << CONTROL_UP_POS_INDEX;			// 0x00000010
constexpr U32 AGENT_CONTROL_UP_NEG			= 0x1 << CONTROL_UP_NEG_INDEX;			// 0x00000020
constexpr U32 AGENT_CONTROL_PITCH_POS		= 0x1 << CONTROL_PITCH_POS_INDEX;		// 0x00000040
constexpr U32 AGENT_CONTROL_PITCH_NEG		= 0x1 << CONTROL_PITCH_NEG_INDEX;		// 0x00000080
constexpr U32 AGENT_CONTROL_YAW_POS			= 0x1 << CONTROL_YAW_POS_INDEX;			// 0x00000100
constexpr U32 AGENT_CONTROL_YAW_NEG			= 0x1 << CONTROL_YAW_NEG_INDEX;			// 0x00000200

constexpr U32 AGENT_CONTROL_FAST_AT			= 0x1 << CONTROL_FAST_AT_INDEX;			// 0x00000400
constexpr U32 AGENT_CONTROL_FAST_LEFT		= 0x1 << CONTROL_FAST_LEFT_INDEX;		// 0x00000800
constexpr U32 AGENT_CONTROL_FAST_UP			= 0x1 << CONTROL_FAST_UP_INDEX;			// 0x00001000

constexpr U32 AGENT_CONTROL_FLY				= 0x1 << CONTROL_FLY_INDEX;				// 0x00002000
constexpr U32 AGENT_CONTROL_STOP			= 0x1 << CONTROL_STOP_INDEX;			// 0x00004000
constexpr U32 AGENT_CONTROL_FINISH_ANIM		= 0x1 << CONTROL_FINISH_ANIM_INDEX;		// 0x00008000
constexpr U32 AGENT_CONTROL_STAND_UP		= 0x1 << CONTROL_STAND_UP_INDEX;		// 0x00010000
constexpr U32 AGENT_CONTROL_SIT_ON_GROUND	= 0x1 << CONTROL_SIT_ON_GROUND_INDEX;	// 0x00020000
constexpr U32 AGENT_CONTROL_MOUSELOOK		= 0x1 << CONTROL_MOUSELOOK_INDEX;		// 0x00040000

constexpr U32 AGENT_CONTROL_NUDGE_AT_POS	= 0x1 << CONTROL_NUDGE_AT_POS_INDEX;	// 0x00080000
constexpr U32 AGENT_CONTROL_NUDGE_AT_NEG	= 0x1 << CONTROL_NUDGE_AT_NEG_INDEX;	// 0x00100000
constexpr U32 AGENT_CONTROL_NUDGE_LEFT_POS	= 0x1 << CONTROL_NUDGE_LEFT_POS_INDEX;	// 0x00200000
constexpr U32 AGENT_CONTROL_NUDGE_LEFT_NEG	= 0x1 << CONTROL_NUDGE_LEFT_NEG_INDEX;	// 0x00400000
constexpr U32 AGENT_CONTROL_NUDGE_UP_POS	= 0x1 << CONTROL_NUDGE_UP_POS_INDEX;	// 0x00800000
constexpr U32 AGENT_CONTROL_NUDGE_UP_NEG	= 0x1 << CONTROL_NUDGE_UP_NEG_INDEX;	// 0x01000000
constexpr U32 AGENT_CONTROL_TURN_LEFT		= 0x1 << CONTROL_TURN_LEFT_INDEX;		// 0x02000000
constexpr U32 AGENT_CONTROL_TURN_RIGHT		= 0x1 << CONTROL_TURN_RIGHT_INDEX;		// 0x04000000

constexpr U32 AGENT_CONTROL_AWAY			= 0x1 << CONTROL_AWAY_INDEX;			// 0x08000000

constexpr U32 AGENT_CONTROL_LBUTTON_DOWN	= 0x1 << CONTROL_LBUTTON_DOWN_INDEX;	// 0x10000000
constexpr U32 AGENT_CONTROL_LBUTTON_UP		= 0x1 << CONTROL_LBUTTON_UP_INDEX;		// 0x20000000
constexpr U32 AGENT_CONTROL_ML_LBUTTON_DOWN	= 0x1 << CONTROL_ML_LBUTTON_DOWN_INDEX;	// 0x40000000
constexpr U32 AGENT_CONTROL_ML_LBUTTON_UP	= ((U32)0x1) << CONTROL_ML_LBUTTON_UP_INDEX;	// 0x80000000

constexpr U32 AGENT_CONTROL_AT 	= AGENT_CONTROL_AT_POS 
								  | AGENT_CONTROL_AT_NEG 
								  | AGENT_CONTROL_NUDGE_AT_POS 
								  | AGENT_CONTROL_NUDGE_AT_NEG;

constexpr U32 AGENT_CONTROL_LEFT = AGENT_CONTROL_LEFT_POS 
								  | AGENT_CONTROL_LEFT_NEG 
								  | AGENT_CONTROL_NUDGE_LEFT_POS 
								  | AGENT_CONTROL_NUDGE_LEFT_NEG;

constexpr U32 AGENT_CONTROL_UP 	= AGENT_CONTROL_UP_POS 
								  | AGENT_CONTROL_UP_NEG 
								  | AGENT_CONTROL_NUDGE_UP_POS 
								  | AGENT_CONTROL_NUDGE_UP_NEG;

constexpr U32 AGENT_CONTROL_HORIZONTAL = AGENT_CONTROL_AT 
										 | AGENT_CONTROL_LEFT;

constexpr U32 AGENT_CONTROL_NOT_USED_BY_LSL = AGENT_CONTROL_FLY 
											  | AGENT_CONTROL_STOP 
											  | AGENT_CONTROL_FINISH_ANIM 
											  | AGENT_CONTROL_STAND_UP 
											  | AGENT_CONTROL_SIT_ON_GROUND 
											  | AGENT_CONTROL_MOUSELOOK 
											  | AGENT_CONTROL_AWAY;

constexpr U32 AGENT_CONTROL_MOVEMENT = AGENT_CONTROL_AT 
									   | AGENT_CONTROL_LEFT 
									   | AGENT_CONTROL_UP;

constexpr U32 AGENT_CONTROL_ROTATION = AGENT_CONTROL_PITCH_POS 
									   | AGENT_CONTROL_PITCH_NEG 
									   | AGENT_CONTROL_YAW_POS 
									   | AGENT_CONTROL_YAW_NEG;

constexpr U32 AGENT_CONTROL_NUDGE = AGENT_CONTROL_NUDGE_AT_POS
									| AGENT_CONTROL_NUDGE_AT_NEG
									| AGENT_CONTROL_NUDGE_LEFT_POS
									| AGENT_CONTROL_NUDGE_LEFT_NEG;
	
// Move these up so that we can hide them in "State" for object updates 
// (for now)
constexpr U32 AGENT_ATTACH_OFFSET	= 4;
constexpr U32 AGENT_ATTACH_MASK		= 0xf << AGENT_ATTACH_OFFSET;

// RN: this method swaps the upper and lower nibbles to maintain backward 
// compatibility with old objects that only used the upper nibble
#define ATTACHMENT_ID_FROM_STATE(state) \
	((S32)((((U8)state & AGENT_ATTACH_MASK) >> 4) | \
	(((U8)state & ~AGENT_ATTACH_MASK) << 4)))

constexpr F32 MAX_ATTACHMENT_DIST	= 3.5f;	// In meters

///////////////////////////////////////////////////////////////////////////////

typedef enum e_camera_modes
{
	CAMERA_MODE_THIRD_PERSON,
	CAMERA_MODE_MOUSELOOK,
	CAMERA_MODE_CUSTOMIZE_AVATAR,
	CAMERA_MODE_FOLLOW
} ECameraMode;

typedef enum e_camera_position
{
	CAMERA_POSITION_SELF,	// Camera positioned at our position
	CAMERA_POSITION_OBJECT	// Camera positioned at observed object's position
} ECameraPosition;

typedef enum e_anim_request
{
	ANIM_REQUEST_START,
	ANIM_REQUEST_STOP
} EAnimRequest;

class LLChat;
class LLFriendObserver;
class LLHost;
class LLMessageSystem;
class LLMotion;
class LLPermissions;
class LLPickInfo;
class LLToolset;
class LLViewerRegion;
class LLVOAvatarSelf;

struct LLGroupData
{
	LLGroupData() = default;

	LLGroupData(const LLUUID& group_id, const std::string& name, U64 powers,
				S32 contribution = 0, bool accept_notices = true,
				bool list_in_profile = true)
	:	mID(group_id),
		mName(name),
		mPowers(powers),
		mContribution(contribution),
		mAcceptNotices(accept_notices),
		mListInProfile(list_in_profile)
	{
	}

	LLUUID		mID;
	LLUUID		mInsigniaID;
	std::string	mName;
	U64			mPowers;
	S32			mContribution;
	bool		mAcceptNotices;
	bool		mListInProfile;
};

LL_INLINE bool operator==(const LLGroupData& a, const LLGroupData& b)
{
	return (a.mID == b.mID);
}

class LLAgent : public LLOldEvents::LLObservable
{
protected:
	LOG_CLASS(LLAgent);

public:
	// When the agent has not typed anything for this duration, it leaves the
	// typing state (for both chat and IM).
	static constexpr F32 TYPING_TIMEOUT_SECS = 5.f;

//MK
	static bool canWear(LLWearableType::EType type);
	static bool canUnwear(LLWearableType::EType type);
//mk

	LLAgent();
	~LLAgent();

	void init();
	void cleanup();

	// MANIPULATORS

	// Called whenever the agent moves. Puts camera back in default position,
	// deselects items, etc.
	void resetView(bool reset_camera = true, bool change_camera = false);

	// Called on camera movement, to allow the camera to be unlocked from the
	// default position behind the avatar.
	void unlockView();

	void onAppFocusGained();

	void sendMessage();					// Send message to this agent's region.
	void sendReliableMessage(U32 retries_factor = 1);

	// Calculate the camera position target
	LLVector3d calcCameraPositionTargetGlobal(bool* hit_limit = NULL);

	LLVector3d calcFocusPositionTargetGlobal();
	LLVector3d calcThirdPersonFocusOffset();
	LLVector3d getCameraPositionGlobal() const;
	const LLVector3& getCameraPositionAgent() const;

	LL_INLINE void resetHUDZoom()
	{
		if (mHUDTargetZoom != 1.f)
		{
			mHUDTargetZoom = mHUDCurZoom = 1.f;
		}
	}

	LL_INLINE void getHUDZoom(F32& target_zoom, F32& current_zoom) const
	{
		target_zoom = mHUDTargetZoom;
		current_zoom = mHUDCurZoom;
	}

	LL_INLINE void setHUDZoom(F32 target_zoom, F32 current_zoom)
	{
		mHUDTargetZoom = target_zoom;
		mHUDCurZoom = current_zoom;
	}

	F32 getHUDTargetZoom() const;

	F32 calcCameraFOVZoomFactor();

	// Minimum height off ground for this mode, meters
	F32 getCameraMinOffGround();

	void endAnimationUpdateUI();

	// Sets key to +1 for +direction, -1 for -direction
	void setKey(S32 direction, S32& key);

	void handleScrollWheel(S32 clicks);	// mousewheel driven zoom

	void setAvatarObject(LLVOAvatarSelf* avatar);

	// Rendering state bitmask helpers
	void startTyping();
	void stopTyping();
	void setRenderState(U8 newstate);
	void clearRenderState(U8 clearstate);
	U8 getRenderState();

	// Set the region data

	typedef boost::signals2::signal<void()> region_change_cb_t;
	boost::signals2::connection	addRegionChangedCB(const region_change_cb_t::slot_type& cb);

	void setRegion(LLViewerRegion* regionp);
	LL_INLINE LLViewerRegion* getRegion() const				{ return mRegionp; }
	U64 getRegionHandle() const;
	const LLHost& getRegionHost() const;
	std::string getSLURL() const;

	bool regionCapabilitiesReceived() const;
	const std::string& getRegionCapability(const char* cap);
	bool hasRegionCapability(const char* cap) const;

	// For OpenSim export permission support. This is on a per-region basis and
	// here we only test the agent region. HB
	bool regionHasExportPermSupport() const;

	// This replaces LLEnvironment::isExtendedEnvironmentEnabled(). HB
	LL_INLINE bool hasExtendedEnvironment() const			{ return mHasExtEnvironment; }
	// This replaces LLEnvironment::isInventoryEnabled(). HB
	LL_INLINE bool hasInventorySettings() const				{ return mInventorySettings; }
	// This replaces LLMaterialEditor::capabilitiesAvailable(). HB
	LL_INLINE bool hasInventoryMaterial() const				{ return mInventoryMaterial; }

	// Call once per frame to update position, angles radians
	void updateAgentPosition(F32 dt, F32 yaw, S32 mouse_x, S32 mouse_y);
	void updateLookAt(S32 mouse_x, S32 mouse_y);

	// Call once per frame to update camera location/orientation
	void updateCamera();
	void resetCamera();		// Slam camera into its default position
	void setupSitCamera();
	void setupCameraView(bool reset = false);

	LL_INLINE void setCameraCollidePlane(const LLVector4& plane)
	{
		mCameraCollidePlane = plane;
	}

	bool changeCameraToDefault(bool animate = true);
	bool changeCameraToMouselook(bool animate = true);
	bool changeCameraToThirdPerson(bool animate = true);
	bool changeCameraToFollow(bool animate = true);
	void changeCameraToCustomizeAvatar();

	void setFocusGlobal(const LLPickInfo& pick);
	void setFocusGlobal(const LLVector3d& focus,
						const LLUUID& object_id = LLUUID::null);
	void setFocusOnAvatar(bool focus = true, bool animate = true);
	void setCameraPosAndFocusGlobal(const LLVector3d& pos,
									const LLVector3d& focus,
									const LLUUID& object_id);
	void setSitCamera(const LLUUID& object_id,
					  const LLVector3& camera_pos = LLVector3::zero,
					  const LLVector3& camera_focus = LLVector3::zero);
	void clearFocusObject();
	void setFocusObject(LLViewerObject* object);
	LL_INLINE void setObjectTracking(bool track)			{ mTrackFocusObject = track; }

	void heardChat(const LLUUID& id);
	void lookAtLastChat();
	void lookAtObject(LLUUID avatar_id, ECameraPosition camera_pos);
	LL_INLINE F32 getTypingTime()							{ return mTypingTimer.getElapsedTimeF32(); }

	void setAFK();
	void clearAFK();
	LL_INLINE bool getAFK() const							{ return (mControlFlags & AGENT_CONTROL_AWAY) != 0; }

	void setBusy();
	void clearBusy();
	LL_INLINE bool getBusy() const							{ return mIsBusy; }

	void setAutoReply();
	void clearAutoReply();
	LL_INLINE bool getAutoReply() const						{ return mIsAutoReplying; }

	LL_INLINE void setAlwaysRun()							{ mAlwaysRun = true; }
	LL_INLINE void clearAlwaysRun()							{ mAlwaysRun = false; }

	LL_INLINE void setRunning()								{ mRunning = true; }
	LL_INLINE void clearRunning()							{ mRunning = false; }

	LL_INLINE void setFirstLogin(bool b)					{ mFirstLogin = b; }
	LL_INLINE void setGenderChosen(bool b)					{ mGenderChosen = b; }

	// update internal datastructures and update the server with the new
	// contribution level. Returns true if the group id was found and
	// contribution could be set.
	bool setGroupContribution(const LLUUID& group_id, S32 contribution);
	bool setUserGroupFlags(const LLUUID& group_id, bool accept_notices,
						   bool list_in_profile);
	LL_INLINE void setHideGroupTitle(bool hide)				{ mHideGroupTitle = hide; }

	void updateLanguage();

	// Note: NEVER send this value in the clear or over any weakly encrypted
	// channel (such as simple XOR masking).
	LL_INLINE const LLUUID& getSecureSessionID() const		{ return mSecureSessionID; }

	//--------------------------------------------------------------------
	// God
	//--------------------------------------------------------------------
	LL_INLINE bool isGodlike() const						{ return mAdminOverride || mGodLevel > GOD_NOT; }
	LL_INLINE bool isGodlikeWithoutAdminMenuFakery() const	{ return mGodLevel > GOD_NOT; }
	LL_INLINE U8 getGodLevel() const						{ return mAdminOverride ? GOD_FULL : mGodLevel; }
	LL_INLINE void setAdminOverride(bool b)					{ mAdminOverride = b; }
	void setGodLevel(U8 god_level);
	void requestEnterGodMode();
	void requestLeaveGodMode();

	typedef boost::function<void (U8)>         god_level_change_callback_t;
	typedef boost::signals2::signal<void (U8)> god_level_change_signal_t;
	typedef boost::signals2::connection        god_level_change_slot_t;

	god_level_change_slot_t registerGodLevelChanageListener(god_level_change_callback_t callback);

	bool wantsPGOnly() const;
	bool canAccessMature() const;
	bool canAccessAdult() const;
	bool canAccessMaturityInRegion(U64 region_handle) const;
	bool canAccessMaturityAtGlobal(LLVector3d pos_global) const;

	bool prefersPG() const;
	bool prefersMature() const;
	bool prefersAdult() const;

	LL_INLINE bool isTeen() const							{ return mAccess < SIM_ACCESS_MATURE; }
	LL_INLINE bool isMature() const							{ return mAccess >= SIM_ACCESS_MATURE; }
	LL_INLINE bool isAdult() const							{ return mAccess >= SIM_ACCESS_ADULT; }

	void setTeen(bool teen);
	void setMaturity(char text);

	static U8 convertTextToMaturity(char text);
	bool sendMaturityPreferenceToServer(U8 preferredMaturity);

	// Maturity callbacks for PreferredMaturity control variable:
	void handleMaturity(const LLSD& newvalue);
	bool validateMaturity(const LLSD& newvalue);

	LL_INLINE bool isGroupTitleHidden() const				{ return mHideGroupTitle; }
	// This is only used for building titles:
	LL_INLINE bool isGroupMember() const					{ return mGroupID.notNull(); }
	LL_INLINE const LLUUID& getGroupID() const				{ return mGroupID; }

	LL_INLINE ECameraMode getCameraMode() const				{ return mCameraMode; }
	LL_INLINE bool getFocusOnAvatar() const					{ return mFocusOnAvatar; }
	LL_INLINE LLPointer<LLViewerObject>& getFocusObject()	{ return mFocusObject; }
	LL_INLINE F32 getFocusObjectDist() const				{ return mFocusObjectDist; }

	bool inPrelude();
	bool canManageEstate() const;
	LL_INLINE bool getAdminOverride() const					{ return mAdminOverride; }

	LL_INLINE LLUUID getLastChatter() const					{ return mLastChatterID; }
	LL_INLINE bool getAlwaysRun() const						{ return mAlwaysRun; }
	LL_INLINE bool getRunning() const						{ return mRunning; }

	void buildFullname(std::string& name) const;
	void buildFullnameAndTitle(std::string& name) const;

	// Checks against all groups in the entire agent group list.
	bool isInGroup(const LLUUID& group_id, bool ignore_god_mode = false) const;
	// Sets to the group (or "none" if group_id is LLUUID::null) if possible
	// and not already in that group. Returns true when the group was changed
	// or already set, false when changing to that group was not possible.
	bool setGroup(const LLUUID& group_id);
	bool hasPowerInGroup(const LLUUID& group_id, U64 power) const;
	// Check for power in just the active group.
	bool hasPowerInActiveGroup(U64 power) const;
	U64 getPowerInGroup(const LLUUID& group_id) const;

	// Get group information by group_id. if not in group, data is left
	// unchanged and method returns false. otherwise, values are copied and
	// returns true.
	bool getGroupData(const LLUUID& group_id, LLGroupData& data) const;
	// Get just the agent's contribution to the given group.
	S32 getGroupContribution(const LLUUID& group_id) const;

	// Returns true if the database reported this login as the first
	// for this particular user.
	LL_INLINE bool isFirstLogin() const						{ return mFirstLogin; }

	// On the very first login, gender isn't chosen until the user clicks
	// in a dialog.  We don't render the avatar until they choose.
	LL_INLINE bool isGenderChosen() const					{ return mGenderChosen; }

	// utility to build a location string
	void buildLocationString(std::string& str);

	LLQuaternion getHeadRotation();

	// true when camera mode is such that your own avatar should draw.
	// Not const because timers can't be accessed in const-fashion.
	bool needsRenderAvatar();

	// true if we need to render your own avatar's head.
	bool needsRenderHead();

	LL_INLINE bool cameraThirdPerson() const				{ return mCameraMode == CAMERA_MODE_THIRD_PERSON && mLastCameraMode == CAMERA_MODE_THIRD_PERSON; }
	LL_INLINE bool cameraMouselook() const					{ return mCameraMode == CAMERA_MODE_MOUSELOOK && mLastCameraMode == CAMERA_MODE_MOUSELOOK; }
	LL_INLINE bool cameraCustomizeAvatar() const			{ return mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR /*&& !mCameraAnimating*/; }
	LL_INLINE bool cameraFollow() const						{ return mCameraMode == CAMERA_MODE_FOLLOW && mLastCameraMode == CAMERA_MODE_FOLLOW; }

	typedef boost::signals2::signal<void(const LLVector3& pos_local,
										 const LLVector3d& pos_global)> pos_change_signal_t;
	boost::signals2::connection setPosChangeCallback(const pos_change_signal_t::slot_type& cb);

	LLVector3 getPosAgentFromGlobal(const LLVector3d& pos_global) const;
	LLVector3d getPosGlobalFromAgent(const LLVector3& pos_agent) const;

	// Get the data members

	// These return the direction the avatar is looking, not the camera's:
	LL_INLINE const LLVector3& getAtAxis() const			{ return mFrameAgent.getAtAxis(); }
	LL_INLINE const LLVector3& getUpAxis() const			{ return mFrameAgent.getUpAxis(); }
	LL_INLINE const LLVector3& getLeftAxis() const			{ return mFrameAgent.getLeftAxis(); }

	LL_INLINE LLCoordFrame getFrameAgent() const			{ return mFrameAgent; }
	LLVector3 getVelocity() const;
	LL_INLINE F32 getVelocityZ() const						{ return getVelocity().mV[VZ]; }

	const LLVector3d& getPositionGlobal() const;
	const LLVector3& getPositionAgent();
	S32 getRegionsVisited() const;
	LL_INLINE F64 getDistanceTraveled() const				{ return mDistanceTraveled; }

	LL_INLINE const LLVector3d&	getFocusGlobal() const		{ return mFocusGlobal; }
	LL_INLINE const LLVector3d&	getFocusTargetGlobal() const
	{
		return mFocusTargetGlobal;
	}

	// Returns the quat that represents the rotation of the agent in the
	// absolute frame:
	LL_INLINE LLQuaternion getQuat() const					{ return mFrameAgent.getQuaternion(); }

	void getName(std::string& name);

	LL_INLINE const LLColor4& getEffectColor()				{ return mEffectColor; }
	LL_INLINE void setEffectColor(const LLColor4& color)	{ mEffectColor = color; }

	//
	// UTILITIES
	//

	// Set the physics data
	void slamLookAt(const LLVector3& look_at);

	void setPositionAgent(const LLVector3& center);

	void resetAxes();
	void resetAxes(const LLVector3& look_at);	// makes reasonable left and up

	// Move the avatar's frame
	void rotate(F32 angle, const LLVector3& axis);
	void rotate(F32 angle, F32 x, F32 y, F32 z);
	void rotate(const LLMatrix3& matrix);
	void rotate(const LLQuaternion& quaternion);
	void pitch(F32 angle);
	void roll(F32 angle);
	void yaw(F32 angle);
	LLVector3 getReferenceUpVector();
    F32 clampPitchToLimits(F32 angle);

	LL_INLINE void setThirdPersonHeadOffset(LLVector3 dlt)	{ mThirdPersonHeadOffset = dlt; }
	// Flight management
	LL_INLINE bool getFlying() const						{ return (mControlFlags & AGENT_CONTROL_FLY) != 0; }
	void setFlying(bool fly, bool play_failed_sound = true);
	void toggleFlying();

	// Does this parcel allow you to fly ?
	bool canFly();

	LL_INLINE bool sittingOnGround()						{ return mSittingOnGround; }
	LL_INLINE void notOnSatGround()							{ mSittingOnGround = false; }

	// Animation functions
	void stopCurrentAnimations();
	void requestStopMotion(LLMotion* motion);
	void onAnimStop(const LLUUID& id);

	void sendAnimationRequests(std::vector<LLUUID>& anim_ids,
							   EAnimRequest request);
	void sendAnimationRequest(const LLUUID& anim_id, EAnimRequest request);
	void sendAnimationStateReset();
	void sendRevokePermissions(const LLUUID& target_id, U32 permissions);

	bool noCameraConstraints();

	LLVector3 calcFocusOffset(LLViewerObject* object, LLVector3 pos_agent,
							  S32 x, S32 y);
	bool calcCameraMinDistance(F32& obj_min_distance);

	void startCameraAnimation();
	LL_INLINE void stopCameraAnimation()				{ mCameraAnimating = false; }

	// Zooms in by fraction of current distance
	void cameraZoomIn(F32 factor);
	// Rotates camera CCW radians about build focus point
	void cameraOrbitAround(F32 radians);
	// Rotates camera forward radians over build focus point
	void cameraOrbitOver(F32 radians);
	// Moves camera in toward build focus point
	void cameraOrbitIn(F32 meters);
	// Gets camera zoom as fraction of minimum and maximum zoom
	F32	 getCameraZoomFraction();
	// Sets camera zoom as fraction of minimum and maximum zoom
	void setCameraZoomFraction(F32 fraction);

	void cameraPanIn(F32 meters);
	void cameraPanLeft(F32 meters);
	void cameraPanUp(F32 meters);

	void updateFocusOffset();
	void validateFocusObject();

	F32 calcCustomizeAvatarUIOffset(const LLVector3d& camera_pos_global);

	// Marks current location as start, sends information to servers
	void setStartPosition(U32 location_id);

	// Movement from user input.  All set the appropriate animation flags.
	// All turn off autopilot and make sure the camera is behind the avatar.
	// direction is either positive, zero, or negative
	void moveAt(S32 direction, bool reset_view = true);
	void moveAtNudge(S32 direction);
	void moveLeft(S32 direction);
	void moveLeftNudge(S32 direction);
	void moveUp(S32 direction);
	void moveYaw(F32 mag, bool reset_view = true);
	void movePitch(S32 direction);

	LL_INLINE void setOrbitLeftKey(F32 mag)					{ mOrbitLeftKey = mag; }
	LL_INLINE void setOrbitRightKey(F32 mag)				{ mOrbitRightKey = mag; }
	LL_INLINE void setOrbitUpKey(F32 mag)					{ mOrbitUpKey = mag; }
	LL_INLINE void setOrbitDownKey(F32 mag)					{ mOrbitDownKey = mag; }
	LL_INLINE void setOrbitInKey(F32 mag)					{ mOrbitInKey = mag; }
	LL_INLINE void setOrbitOutKey(F32 mag)					{ mOrbitOutKey = mag; }

	LL_INLINE void setPanLeftKey(F32 mag)					{ mPanLeftKey = mag; }
	LL_INLINE void setPanRightKey(F32 mag)					{ mPanRightKey = mag; }
	LL_INLINE void setPanUpKey(F32 mag)						{ mPanUpKey = mag; }
	LL_INLINE void setPanDownKey(F32 mag)					{ mPanDownKey = mag; }
	LL_INLINE void setPanInKey(F32 mag)						{ mPanInKey = mag; }
	LL_INLINE void setPanOutKey(F32 mag)					{ mPanOutKey = mag; }

	LL_INLINE U32 getControlFlags()							{ return mControlFlags; }
	// Performs bitwise mControlFlags |= mask
	void setControlFlags(U32 mask);
	// Performs bitwise mControlFlags &= ~mask
	void clearControlFlags(U32 mask);
	LL_INLINE bool controlFlagsDirty() const				{ return mFlagsDirty; }
	LL_INLINE void enableControlFlagReset()					{ mFlagsNeedReset = true; }
	void resetControlFlags();

	// *FIXME: should roll into updateAgentPosition
	void propagate(F32 dt);

	//
	// Teportation methods
	//

	// Fire any queued, waiting teleport, if possible. HB
	void fireQueuedTeleport();

	// Teleport to a landmark
	void teleportViaLandmark(const LLUUID& landmark_id);

	// Go home
	LL_INLINE void teleportHome()							{ teleportViaLandmark(LLUUID::null); }

	// Teleport to an invited location
	void teleportViaLure(const LLUUID& lure_id, bool godlike);

	// Teleport to a global location
	void teleportViaLocation(const LLVector3d& pos_global);

	// Teleport to a global location, preserving camera rotation
	void teleportViaLocationLookAt(const LLVector3d& pos_global);

	// Cancel the teleport, may or may not be allowed by server
	void teleportCancel();

	LL_INLINE const std::string getTeleportSourceSLURL() const
	{
		return mTeleportSourceSLURL;
	}

	void setTargetVelocity(const LLVector3& vel);
	const LLVector3& getTargetVelocity() const;

	void handleServerFeaturesTransition();

	static void processAgentDataUpdate(LLMessageSystem* msg, void**);
	static void processAgentGroupDataUpdate(LLMessageSystem* msg, void**);
	static void processAgentDropGroup(LLMessageSystem* msg, void**);
	static void processScriptControlChange(LLMessageSystem* msg, void**);
	static void processAgentCachedTextureResponse(LLMessageSystem* mesgsys,
												  void** user_data);

	// This method checks to see if this agent can modify an object based on
	// the permissions and the agent's proxy status.
	bool isGrantedProxy(const LLPermissions& perm);

	bool allowOperation(PermissionBit op, const LLPermissions& perm,
						U64 group_proxy_power = 0,
						U8 god_minimum = GOD_MAINTENANCE);

	friend std::ostream& operator<<(std::ostream& s, const LLAgent& sphere);

	// Only to be used in ONE place ! - djs 08/07/02
	LL_INLINE void initOriginGlobal(const LLVector3d& pos)	{ mAgentOriginGlobal = pos; }

	LL_INLINE bool leftButtonGrabbed() const
	{
		if (cameraMouselook())
		{
			return mControlsTakenCount[CONTROL_ML_LBUTTON_DOWN_INDEX] > 0 ||
				   mControlsTakenPassedOnCount[CONTROL_ML_LBUTTON_DOWN_INDEX] > 0;
		}
		else
		{
			return mControlsTakenCount[CONTROL_LBUTTON_DOWN_INDEX] > 0 ||
				   mControlsTakenPassedOnCount[CONTROL_LBUTTON_DOWN_INDEX] > 0;
		}
	}

	LL_INLINE bool rotateGrabbed() const
	{
		return mControlsTakenCount[CONTROL_YAW_POS_INDEX] > 0 ||
			   mControlsTakenCount[CONTROL_YAW_NEG_INDEX] > 0;
	}

	LL_INLINE bool forwardGrabbed() const					{ return mControlsTakenCount[CONTROL_AT_POS_INDEX] > 0; }
	LL_INLINE bool backwardGrabbed() const					{ return mControlsTakenCount[CONTROL_AT_NEG_INDEX] > 0; }
	LL_INLINE bool upGrabbed() const						{ return mControlsTakenCount[CONTROL_UP_POS_INDEX] > 0; }
	LL_INLINE bool downGrabbed() const						{ return mControlsTakenCount[CONTROL_UP_NEG_INDEX] > 0; }

	// True if a script has taken over a control.
	bool			anyControlGrabbed() const;

	LL_INLINE bool isControlGrabbed(S32 ctrl_index) const	{ return mControlsTakenCount[ctrl_index] > 0; }

	// Send message to simulator to force grabbed controls to be
	// released, in case of a poorly written script.
	void			forceReleaseControls();

	LL_INLINE bool sitCameraEnabled()						{ return mSitCameraEnabled; }

	LL_INLINE F32 getCurrentCameraBuildOffset()				{ return (F32)mCameraFocusOffset.length(); }

	LL_INLINE ELookAtType getLookAtType()
	{
		return mLookAt ? mLookAt->getLookAtType() : LOOKAT_TARGET_NONE;
	}

	LL_INLINE EPointAtType getPointAtType()
	{
		return mPointAt ? mPointAt->getPointAtType() : POINTAT_TARGET_NONE;
	}

	// look at behavior.
	bool setLookAt(ELookAtType target_type, LLViewerObject* object = NULL,
				   LLVector3 position = LLVector3::zero);
	bool setPointAt(EPointAtType target_type, LLViewerObject* object = NULL,
					LLVector3 position = LLVector3::zero);

	void setHomePosRegion(const U64& region_handle,
						  const LLVector3& pos_region);
	bool getHomePosGlobal(LLVector3d* pos_global);

	LL_INLINE void setCameraAnimating(bool b)				{ mCameraAnimating = b; }
	LL_INLINE bool getCameraAnimating()						{ return mCameraAnimating; }
	LL_INLINE void setAnimationDuration(F32 seconds)		{ mAnimationDuration = seconds; }

	LL_INLINE F32 getNearChatRadius()						{ return mNearChatRadius; }
	void setNearChatRadius(F32 radius);

	enum EDoubleTapRunMode
	{
		DOUBLETAP_NONE,
		DOUBLETAP_FORWARD,
		DOUBLETAP_BACKWARD,
		DOUBLETAP_SLIDELEFT,
		DOUBLETAP_SLIDERIGHT
	};

	enum ETeleportState
	{
		TELEPORT_NONE = 0,	// No teleport in progress
		// Transition to REQUESTED. Viewer has sent a TeleportRequest to the source
		// simulator.
		TELEPORT_START = 1,
		// Waiting for source simulator to respond with TeleportFinish.
		TELEPORT_REQUESTED = 2,
		// Viewer has received destination location from source simulator
		TELEPORT_MOVING = 3,
		// Transition to ARRIVING. Viewer has received avatar update, etc, from
		// destination simulator
		TELEPORT_START_ARRIVAL = 4,
		// Make the user wait while content "pre-caches"
		TELEPORT_ARRIVING = 5,
		// Teleporting in-sim without showing the progress screen
		TELEPORT_LOCAL = 6,
		// Viewer not yet ready to receive reliably the TeleportFinish message:
		// TP has been queued. HB
		TELEPORT_QUEUED = 10,
	};

	LL_INLINE ETeleportState getTeleportState() const		{ return mTeleportState; }
	void setTeleportState(ETeleportState state,
						  const std::string& reason = LLStringUtil::null);

	LL_INLINE bool notTPingFar()
	{
		return mTeleportState == TELEPORT_NONE ||
			   mTeleportState == TELEPORT_LOCAL;
	}

	LL_INLINE bool teleportInProgress()
	{
		return mTeleportState != TELEPORT_NONE;
	}

	LL_INLINE const std::string& getTeleportMessage() const	{ return mTeleportMessage; }
	LL_INLINE void setTeleportMessage(const std::string& m)	{ mTeleportMessage = m; }

	LL_INLINE bool wasTeleportedFar()						{ return mArrivalHandle != mDepartureHandle; }

	// Whether look-at reset after teleport
	LL_INLINE bool getTeleportKeepsLookAt()					{ return mTeleportKeepsLookAt; }

	LL_INLINE U64 getTeleportedSimHandle()					{ return mTeleportedSimHandle; }
	LL_INLINE const LLVector3d& getTeleportedPosGlobal()	{ return mTeleportedPosGlobal; }

	static void parseTeleportMessages(const std::string& xml_filename);

	// Triggers random fidget animations
	void fidget();

	// Returns true when a rebake has been scheduled as a result of a change
	// in the number of uploaded bakes (which may now only happen in OpenSim).
	bool setUploadedBakesLimit();

	void sendAgentSetAppearance();

	void sendAgentDataUpdateRequest();
	void sendAgentUpdateUserInfo(bool im_via_email,
								 const std::string& dir_visibility);
	void sendAgentUserInfoRequest();

	void sendWalkRun(bool running);

	void observeFriends();
	void friendsChanged();

	static void stopFidget();
	static void clearVisualParams(void*);	// debug method

	typedef LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t httpCallback_t;

	bool requestPostCapability(const char* cap_name, LLSD& data,
							   httpCallback_t cbsucc = NULL,
							   httpCallback_t cbfail = NULL);
	bool requestGetCapability(const char* cap_name,
							  httpCallback_t cbsucc = NULL,
							  httpCallback_t cbfail = NULL);

	LL_INLINE LLCore::HttpRequest::policy_t getAgentPolicy() const
	{
		return mHttpPolicy;
	}

protected:
	// Helper function to prematurely age chat when agent is moving
	void ageChat();

private:
	bool canSetMaturity(U8 maturity);

	static void setStartPositionSuccess(const LLSD& result);
	static void processMaturityPreferenceFromServer(const LLSD& result,
													std::string reqmatstr);
	static void handlePreferredMaturityError(U8 requested_maturity);

	static void userInfoRequestCallback(const LLSD& result, bool success);
	void sendAgentUserInfoRequestMessage();

	static void userInfoUpdateCallback(const LLSD& result, bool success,
									   bool im_via_email, std::string dir_vis);
	void sendAgentUserInfoRequestMessage(bool im_via_email,
										 const std::string& dir_vis);

	void checkPositionChanged();

	void setTeleportedSimHandle(const LLVector3d& pos_global);
	void resetTeleportedSimHandle();

	// Stuff to do for any sort of teleport. Returns true if the teleport can
	// proceed.
	bool teleportCore(const LLVector3d& pos_global = LLVector3d::zero);

	// Keeps camera look-at axis when look_at vector is zero
	void teleportRequest(U64 region_handle, const LLVector3d& pos_global,
						 const LLVector3& pos_local,
						 const LLVector3& look_at = LLVector3::zero);

public:
	// Secure token for this login session
	LLUUID							mSecureSessionID;

	LLUUID							mGroupID;
	std::string						mGroupName;
	std::string						mGroupTitle;
	U64								mGroupPowers;
	std::vector<LLGroupData>		mGroups;


	// Message of the day
	std::string						mMOTD;

	LLUUID							mMapID;
	S32								mMapWidth;		// Width of map in meters
	S32								mMapHeight;		// Height of map in meters
	// Global x coord of mMapID's bottom left corner.
	F64								mMapOriginX;
	// Global y coord of mMapID's bottom left corner.
	F64								mMapOriginY;

	LLPointer<LLHUDEffectLookAt>	mLookAt;
	LLPointer<LLHUDEffectPointAt>	mPointAt;

	F32								mDrawDistance;

	// Current animated zoom level for HUD objects
	F32								mHUDCurZoom;

	LLFollowCam						mFollowCam;

	LLFrameTimer 					mDoubleTapRunTimer;
	EDoubleTapRunMode				mDoubleTapRunMode;

	U8								mUploadedBakes;
	bool							mRebakeNeeded;

	bool							mInitialized;
	bool							mForceMouselook;
	bool							mHideGroupTitle;

	// We should really define ERROR and PROGRESS enums here but I do not
	// really feel like doing that, so I am just going to expose the
	// mappings... yup
	typedef std::map<std::string, std::string> tp_msg_map_t;
	static tp_msg_map_t				sTeleportErrorMessages;
	static tp_msg_map_t				sTeleportProgressMessages;

private:
	std::string						mTeleportMessage;
	ETeleportState					mTeleportState;

	U64								mDepartureHandle;	// Set when TP starts
	U64								mArrivalHandle;		// Set on TP arrival
	U64								mTeleportedSimHandle;
	// SLURL where last TP began.
	std::string						mTeleportSourceSLURL;
	LLVector3d						mTeleportedPosGlobal;

	// Target zoom level for HUD objects (used when editing)
	F32								mHUDTargetZoom;

	S32								mControlsTakenCount[TOTAL_CONTROLS];
	S32								mControlsTakenPassedOnCount[TOTAL_CONTROLS];

	god_level_change_signal_t		mGodLevelChangeSignal;
	LLCore::HttpRequest::policy_t	mHttpPolicy;

	LLViewerRegion*					mRegionp;

	pos_change_signal_t				mPosChangeSignal;
	// Origin of agent coords from global coords
	LLVector3d						mAgentOriginGlobal;
	mutable LLVector3d				mPositionGlobal;
	// Used to calculate travel distance
	LLVector3d						mLastPositionGlobal;
	// Used to detect position changes
	LLVector3d						mLastPosGlobalTest;
	// Position at TP departure point
	LLVector3d						mPosGlobalTPdeparture;
	// How far has the avatar moved
	F64								mDistanceTraveled;
	// Last time the position change was signaled
	F32								mLastPosGlobalSignaled;

	// Statistsics: what distinct regions has the avatar been to ?
	std::set<U64>					mRegionsVisited;

	LLFrameTimer					mTypingTimer;

	// Current behavior state of agent
	U8								mRenderState;

	// Target mode after transition animation is done
	ECameraMode						mCameraMode;
	ECameraMode						mLastCameraMode;
	LLAnimPauseRequest mPauseRequest;
	U32								mAppearanceSerialNum;
	// Camera start position, global coords
	LLVector3d						mAnimationCameraStartGlobal;
	// Camera focus point, global coords
	LLVector3d						mAnimationFocusStartGlobal;
	// Seconds that transition animation has been active
	LLFrameTimer					mAnimationTimer;
	F32								mAnimationDuration;		// Seconds
	// Amount of fov zoom applied to camera when zeroing in on an object
	F32								mCameraFOVZoomFactor;
	// Interpolated fov zoom
	F32								mCameraCurrentFOVZoomFactor;
	// Offset from focus point in build mode
	LLVector3d						mCameraFocusOffset;
	// Target towards which we are lerping the camera's focus offset
	LLVector3d						mCameraFocusOffsetTarget;
	// Default focus point offset relative to avatar
	LLVector3						mCameraFocusOffsetDefault;
	// Default third-person camera offset
	LLVector3						mCameraOffsetDefault;
	// Colliding plane for camera
	LLVector4						mCameraCollidePlane;
	// Current camera offset from avatar
	F32								mCurrentCameraDistance;
	// Target camera offset from avatar
	F32								mTargetCameraDistance;
	// Mouse wheel driven fraction of zoom
	F32								mCameraZoomFraction;
	// Third person camera lag
	LLVector3						mCameraLag;
	// Head offset for third person camera position
	LLVector3						mThirdPersonHeadOffset;
	// Camera position in agent coordinates
	LLVector3						mCameraPositionAgent;
	// Camera virtual position (target) before performing FOV zoom
	LLVector3						mCameraVirtualPositionAgent;
	// Root relative camera pos when sitting
	LLVector3						mSitCameraPos;
	// Root relative camera target when sitting
	LLVector3						mSitCameraFocus;
	LLVector3d						mCameraSmoothingLastPositionGlobal;
	LLVector3d						mCameraSmoothingLastPositionAgent;

	// Camera's up direction in world coordinates (determines the 'roll' of the
	// view):
	LLVector3						mCameraUpVector;

	// Object to which camera is related when sitting
	LLPointer<LLViewerObject>	mSitCameraReferenceObject;

	LLPointer<LLViewerObject>		mFocusObject;
	LLVector3d						mFocusGlobal;
	LLVector3d						mFocusTargetGlobal;
	LLVector3						mFocusObjectOffset;
	F32								mFocusObjectDist;
	F32								mUIOffset;

	// Agent position and view, agent-region coordinates
	LLCoordFrame					mFrameAgent;

	// Either 1, 0, or -1... Indicates that movement-key is pressed
	S32 							mAtKey;
	// Like AtKey, but causes less forward thrust
	S32								mWalkKey;
	S32 							mLeftKey;
	S32								mUpKey;
	F32								mYawKey;
	S32								mPitchKey;

	F32								mOrbitLeftKey;
	F32								mOrbitRightKey;
	F32								mOrbitUpKey;
	F32								mOrbitDownKey;
	F32								mOrbitInKey;
	F32								mOrbitOutKey;

	F32								mPanUpKey;
	F32								mPanDownKey;
	F32								mPanLeftKey;
	F32								mPanRightKey;
	F32								mPanInKey;
	F32								mPanOutKey;

	U32								mControlFlags;

	LLFriendObserver*				mFriendObserver;
	std::set<LLUUID>				mProxyForAgents;

	LLColor4						mEffectColor;

	LLVector3						mHomePosRegion;
	U64								mHomeRegionHandle;
	LLFrameTimer					mChatTimer;
	LLUUID							mLastChatterID;
	F32								mNearChatRadius;

	LLFrameTimer					mFidgetTimer;
	LLFrameTimer					mFocusObjectFadeTimer;
	F32								mNextFidgetTime;
	S32								mCurrentFidget;

	// Emitted when agent region changes.
	region_change_cb_t				mRegionChangeSignal;

	// Used to be part of the (useless and now removed) LLAgentAccess class...
	U8								mAccess;	// SIM_ACCESS_MATURE etc
	U8								mGodLevel;
	bool							mAdminOverride;

	// Keep track of whether or not we have pushed views.
	bool							mViewsPushed;
	// Try to keep look-at after teleport is complete
	bool							mTeleportKeepsLookAt;
	// The avatar runs by default rather than walking
	bool							mAlwaysRun;
	// Is the avatar trying to run right now ?
	bool							mRunning;

	bool							mSittingOnGround;

	// Current animation is ANIM_AGENT_CUSTOMIZE ?
	bool							mCustomAnim;
	// Should we render the avatar?
	bool							mShowAvatar;
	// Camera is transitioning from one mode to another
	bool							mCameraAnimating;

	// Use provided camera information when sitting?
	bool							mSitCameraEnabled;
	bool							mCameraSmoothingStop;

	bool							mFlagsDirty;
	// HACK for preventing incorrect flags sent when crossing region boundaries
	bool							mFlagsNeedReset;

	bool							mFocusOnAvatar;
	bool							mTrackFocusObject;

	bool							mIsBusy;
	bool							mIsAutoReplying;

	// Capability flags caches
	bool							mHasExtEnvironment;
	// Inventory items support in the agent region.
	bool							mInventorySettings;
	bool							mInventoryMaterial;

	bool							mHaveHomePosition;

	bool							mFirstLogin;
	bool							mGenderChosen;
};

class LLAgentQueryManager
{
	friend class LLAgent;
	friend class LLAgentWearables;

public:
	LLAgentQueryManager();

	LL_INLINE bool 	hasNoPendingQueries() const 			{ return getNumPendingQueries() == 0; }
	LL_INLINE S32 	getNumPendingQueries() const 			{ return mNumPendingQueries; }
	LL_INLINE void	resetPendingQueries()					{ mNumPendingQueries = 0; }

private:
	S32				mNumPendingQueries;
	S32				mWearablesCacheQueryID;
	U32				mUpdateSerialNum;
	S32		    	mActiveCacheQueries[LLAvatarAppearanceDefines::BAKED_NUM_INDICES];
};

extern LLAgent gAgent;
extern LLUUID gAgentID;
extern LLUUID gAgentSessionID;
extern LLAgentQueryManager gAgentQueryManager;

void update_group_floaters(const LLUUID& group_id);

#endif
