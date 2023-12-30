/**
 * @file llagent.cpp
 * @brief LLAgent class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llagent.h"

#include "imageids.h"
#include "llanimationstates.h"
#include "llapp.h"
#include "llappearancemgr.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llconsole.h"
#include "llevent.h"
#include "llexperiencecache.h"
#include "llimage.h"					// For activateStaleTextures()
#include "llmenugl.h"
#include "llmd5.h"
#include "llparcel.h"
#include "llpermissions.h"
#include "llregionhandle.h"
#include "llscriptpermissions.h"
#include "llsdutil.h"
#include "llsys.h"
#include "llteleportflags.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llmessage.h"
#include "roles_constants.h"

#include "llagentpilot.h"
#include "llagentwearables.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "lldrawable.h"
#include "lleventpoll.h"				// For LLEventPoll::getMargin()
#include "llface.h"
#include "llfirstuse.h"
#include "llfloater.h"
#include "llfloateractivespeakers.h"
#include "llfloateravatarinfo.h"
#include "llfloatercamera.h"
#include "llfloaterchat.h"
#include "llfloatercustomize.h"
#include "llfloatergroupinfo.h"
#include "llfloatergroups.h"
#include "llfloaterland.h"
#include "llfloaterminimap.h"
#include "llfloatermove.h"
#include "llfloaterpostcard.h"
#include "llfloaterpreference.h"
#include "hbfloatersearch.h"
#include "llfloatersnapshot.h"
#include "hbfloaterteleporthistory.h"
#include "llfloatertools.h"
#include "llfloaterworldmap.h"
#include "llfollowcam.h"
#include "llgridmanager.h"
#include "llgroupmgr.h"
#include "llhudeffectlookat.h"
#include "llhudmanager.h"
#include "llimmgr.h"
#include "llinventorymodel.h"
#include "lljoystickbutton.h"
#include "lllandmarklist.h"
#include "llmarketplacefunctions.h"
#include "llmeshrepository.h"
#include "llmorphview.h"
#include "llpipeline.h"
#include "llpuppetmotion.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llslurl.h"
#include "llstatusbar.h"
#include "llstartup.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolview.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gTeleportDisplay
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"		// For gTextureList
#include "llviewerwindow.h"
#include "llviewerdisplay.h"
#include "llvoavatarself.h"
#include "llvosky.h"
#include "llwearablelist.h"
#include "llworld.h"
#include "llworldmap.h"

using namespace LLOldEvents;
using namespace LLAvatarAppearanceDefines;

LLUUID gAgentID;
LLUUID gAgentSessionID;

// Face editing constants
const LLVector3d FACE_EDIT_CAMERA_OFFSET(0.4f, -0.05f, 0.07f);
const LLVector3d FACE_EDIT_TARGET_OFFSET(0.f, 0.f, 0.05f);

// Mousewheel camera zoom
constexpr F32 MIN_ZOOM_FRACTION = 0.25f;
constexpr F32 INITIAL_ZOOM_FRACTION = 1.f;
constexpr F32 MAX_ZOOM_FRACTION = 8.f;

constexpr F32 CAMERA_ZOOM_HALF_LIFE = 0.07f;	// In seconds
constexpr F32 FOV_ZOOM_HALF_LIFE = 0.07f;		// In seconds

constexpr F32 CAMERA_FOCUS_HALF_LIFE = 0.f;		// 0.02f;
constexpr F32 CAMERA_LAG_HALF_LIFE = 0.25f;
constexpr F32 MIN_CAMERA_LAG = 0.5f;
constexpr F32 MAX_CAMERA_LAG = 5.f;

constexpr F32 CAMERA_COLLIDE_EPSILON = 0.1f;
constexpr F32 MIN_CAMERA_DISTANCE = 0.1f;
constexpr F32 AVATAR_ZOOM_MIN_X_FACTOR = 0.55f;
constexpr F32 AVATAR_ZOOM_MIN_Y_FACTOR = 0.7f;
constexpr F32 AVATAR_ZOOM_MIN_Z_FACTOR = 1.15f;

constexpr F32 MAX_CAMERA_DISTANCE_FROM_AGENT = 50.f;

constexpr F32 MAX_CAMERA_SMOOTH_DISTANCE = 50.f;

constexpr F32 HEAD_BUFFER_SIZE = 0.3f;
constexpr F32 CUSTOMIZE_AVATAR_CAMERA_ANIM_SLOP = 0.2f;

constexpr F32 LAND_MIN_ZOOM = 0.15f;
constexpr F32 AVATAR_MIN_ZOOM = 0.5f;
constexpr F32 OBJECT_MIN_ZOOM = 0.02f;

constexpr F32 APPEARANCE_MIN_ZOOM = 0.39f;
constexpr F32 APPEARANCE_MAX_ZOOM = 8.f;

// Fidget constants in seconds
constexpr F32 MIN_FIDGET_TIME = 8.f;
constexpr F32 MAX_FIDGET_TIME = 20.f;

constexpr F32 GROUND_TO_AIR_CAMERA_TRANSITION_TIME = 0.5f;
constexpr F32 GROUND_TO_AIR_CAMERA_TRANSITION_START_TIME = 0.5f;

constexpr F32 MAX_VELOCITY_AUTO_LAND_SQUARED = 4.f * 4.f;

constexpr F32 OBJECT_EXTENTS_PADDING = 0.5f;

constexpr F64 CHAT_AGE_FAST_RATE = 3.0;

// The agent instance.
LLAgent gAgent;

// Static member variables
std::map<std::string, std::string> LLAgent::sTeleportErrorMessages;
std::map<std::string, std::string> LLAgent::sTeleportProgressMessages;

// Friends observer

class LLAgentFriendObserver final : public LLFriendObserver
{
public:
	LLAgentFriendObserver()
	{
	}

	~LLAgentFriendObserver() override
	{
	}

	void changed(U32 mask) override
	{
		// If there is a change we are interested in.
		if ((mask & LLFriendObserver::POWERS) != 0)
		{
			gAgent.friendsChanged();
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
// This used to be a template in its own llcommon/llsmoothstep.h header, but
// since it is only used here and only with F32 as a data type, I moved it here
// and made it into a simple, non-template function. HB
///////////////////////////////////////////////////////////////////////////////
LL_INLINE F32 llsmoothstep(F32 edge0, F32 edge1, F32 value)
{
    if (value < edge0)
	{
		return 0.f;
	}

	if (value >= edge1)
	{
		return 1.f;
	}

	// Scale/bias into [0..1] range
	F32 scaled_value = (value - edge0) / (edge1 - edge0);

	return scaled_value * scaled_value * (3.f - 2.f * scaled_value);
}

//-----------------------------------------------------------------------------
// LLAgent() class
//-----------------------------------------------------------------------------

LLAgent::LLAgent()
:	mDrawDistance(DEFAULT_FAR_PLANE),

	mGroupPowers(0),
	mHideGroupTitle(false),

	mMapOriginX(0.F),
	mMapOriginY(0.F),
	mMapWidth(0),
	mMapHeight(0),

	mLookAt(NULL),
	mPointAt(NULL),

	mHUDTargetZoom(1.f),
	mHUDCurZoom(1.f),
	mInitialized(false),
	mUploadedBakes(BAKED_HAIR + 1),
	mRebakeNeeded(false),
	mForceMouselook(false),

	mDoubleTapRunMode(DOUBLETAP_NONE),

	mAlwaysRun(false),
	mRunning(false),

	mAccess(SIM_ACCESS_PG),
	mAdminOverride(false),
	mGodLevel(GOD_NOT),

	mHttpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID),

	mTeleportState(TELEPORT_NONE),
	mRegionp(NULL),
	mDepartureHandle(0),
	mArrivalHandle(0),
	mLastPosGlobalSignaled(0.f),

	mDistanceTraveled(0.0),

	mRenderState(0),

	mCameraMode(CAMERA_MODE_THIRD_PERSON),
	mLastCameraMode(CAMERA_MODE_THIRD_PERSON),
	mViewsPushed(false),

	mCustomAnim(false),
	mShowAvatar(true),
	mCameraAnimating(false),
	mAnimationDuration(0.33f),

	mCameraFOVZoomFactor(0.f),
	mCameraCurrentFOVZoomFactor(0.f),

	mCurrentCameraDistance(2.f),		// meters, set in init()
	mTargetCameraDistance(2.f),
	mCameraZoomFraction(1.f),			// deprecated
	mThirdPersonHeadOffset(0.f, 0.f, 1.f),
	mSitCameraEnabled(false),
	mCameraSmoothingStop(false),

	mCameraUpVector(LLVector3::z_axis), // default is straight up

	mFocusOnAvatar(true),
	mFocusObject(NULL),
	mFocusObjectDist(0.f),
	mTrackFocusObject(true),
	mUIOffset(0.f),

	mIsBusy(false),
	mIsAutoReplying(false),

	mHasExtEnvironment(false),
	mInventorySettings(false),
	mInventoryMaterial(false),

	mAtKey(0), // Either 1, 0, or -1... indicates that movement-key is pressed
	mWalkKey(0), // like AtKey, but causes less forward thrust
	mLeftKey(0),
	mUpKey(0),
	mYawKey(0.f),
	mPitchKey(0),

	mOrbitLeftKey(0.f),
	mOrbitRightKey(0.f),
	mOrbitUpKey(0.f),
	mOrbitDownKey(0.f),
	mOrbitInKey(0.f),
	mOrbitOutKey(0.f),

	mPanUpKey(0.f),
	mPanDownKey(0.f),
	mPanLeftKey(0.f),
	mPanRightKey(0.f),
	mPanInKey(0.f),
	mPanOutKey(0.f),

	mControlFlags(0x00000000),
	mFlagsDirty(false),
	mFlagsNeedReset(false),
	mSittingOnGround(false),

	mEffectColor(0.f, 1.f, 1.f, 1.f),

	mHaveHomePosition(false),
	mHomeRegionHandle(0),
	mNearChatRadius(CHAT_NORMAL_RADIUS * 0.5f),

	mNextFidgetTime(0.f),
	mCurrentFidget(0),
	mFirstLogin(false),
	mGenderChosen(false),
	mAppearanceSerialNum(0),
	mTeleportKeepsLookAt(false)
{
	for (U32 i = 0; i < TOTAL_CONTROLS; ++i)
	{
		mControlsTakenCount[i] = 0;
		mControlsTakenPassedOnCount[i] = 0;
	}

	mFollowCam.setMaxCameraDistantFromSubject(MAX_CAMERA_DISTANCE_FROM_AGENT);
}

// Requires gSavedSettings to be initialized.
void LLAgent::init()
{
	// Initialize the appearance dictionary before we need it... This saves us
	// having to use a slow and cumbersome LLSingleton to access the pointer to
	// this class.
	gAvatarAppDictp =
		new LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary();

	mDrawDistance = gSavedSettings.getF32("RenderFarClip");

	// Let's initialize the camera now...
	gViewerCamera.initClass();
	gViewerCamera.setView(DEFAULT_FIELD_OF_VIEW);
	// Leave at 0.1 meters until we have real near clip management
	gViewerCamera.setNear(0.1f);
	// If you want to change camera settings, do so in camera.h
	gViewerCamera.setFar(mDrawDistance);
	// Default, overridden in LLViewerWindow::reshape
	gViewerCamera.setAspect(gViewerWindowp->getDisplayAspectRatio());
	// Default, overridden in LLViewerWindow::reshape
	gViewerCamera.setViewHeightInPixels(768);

	setFlying(gSavedSettings.getBool("FlyingAtExit"));

	mCameraFocusOffsetTarget =
		LLVector4(gSavedSettings.getVector3("CameraOffsetBuild"));
	mCameraOffsetDefault = gSavedSettings.getVector3("CameraOffsetDefault");
	mCameraFocusOffsetDefault =
		gSavedSettings.getVector3("FocusOffsetDefault");
	mCameraCollidePlane.clear();
	mCurrentCameraDistance = mCameraOffsetDefault.length() *
							 gSavedSettings.getF32("CameraOffsetScale");
	mTargetCameraDistance = mCurrentCameraDistance;
	mCameraZoomFraction = 1.f;
	mTrackFocusObject = gSavedSettings.getBool("TrackFocusObject");

	mEffectColor = gSavedSettings.getColor4("EffectColor");

	LLControlVariable* maturity =
		gSavedSettings.getControl("PreferredMaturity");
	if (maturity)
	{
		maturity->getValidateSignal()->connect(boost::bind(&LLAgent::validateMaturity,
														   this, _2));
		maturity->getSignal()->connect(boost::bind(&LLAgent::handleMaturity,
												   this, _2));
	}

	LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
	app_core_http.getPolicy(LLAppCoreHttp::AP_AGENT);

	mInitialized = true;
}

void LLAgent::cleanup()
{
	setSitCamera(LLUUID::null);
	if (mLookAt)
	{
		mLookAt->markDead();
		mLookAt = NULL;
	}
	if (mPointAt)
	{
		mPointAt->markDead();
		mPointAt = NULL;
	}
	mRegionp = NULL;
	setFocusObject(NULL);
}

LLAgent::~LLAgent()
{
	cleanup();
}

// Change camera back to third person, stop the autopilot, deselect stuff, etc.
void LLAgent::resetView(bool reset_camera, bool change_camera)
{
	static bool dont_reenter = false;
	if (dont_reenter) return;

//MK
	if (gRLenabled && mCameraMode != CAMERA_MODE_MOUSELOOK &&
		gRLInterface.mCamDistMax <= 0.f)
	{
		changeCameraToMouselook(false);
		return;
	}
//mk

	dont_reenter = true;

	bool was_not_customizing = mCameraMode != CAMERA_MODE_CUSTOMIZE_AVATAR;

	gAgentPilot.stopAutoPilot(true);

	gSelectMgr.unhighlightAll();

#if 0	// By popular request, keep land selection while walking around. JC
	gViewerParcelMgr.deselectLand();
#endif

	// Force deselect when walking and attachment is selected; this is so
	// people do not wig out when their avatar moves without animating
	if (gSelectMgr.getSelection()->isAttachment())
	{
		gSelectMgr.deselectAll();
	}

	// Hide all popup menus
	if (gMenuHolderp)
	{
		gMenuHolderp->hideMenus();
	}

	if (change_camera && !LLPipeline::sFreezeTime)
	{
		changeCameraToDefault();

		LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
		if (joystick->getOverrideCamera())
		{
			joystick->toggleFlycam();
		}
		// Reset avatar mode from eventual residual motion
		if (gToolMgr.inBuildMode())
		{
			joystick->moveAvatar(true);
		}

		if (gFloaterToolsp)
		{
			gFloaterToolsp->close();
		}

		if (gViewerWindowp)
		{
			gViewerWindowp->showCursor();
		}

		// Switch back to basic toolset
		gToolMgr.setCurrentToolset(gBasicToolset);
	}

	if (reset_camera && !LLPipeline::sFreezeTime &&
		(was_not_customizing ||
		 gSavedSettings.getBool("AppearanceAnimation") ||
		 gSavedSettings.getBool("AppearanceCameraMovement")))
	{
		if (gViewerWindowp && !gViewerWindowp->getLeftMouseDown() &&
			cameraThirdPerson())
		{
			// Leaving mouse-steer mode
			LLVector3 agent_at_axis = getAtAxis();
			agent_at_axis -= projected_vec(agent_at_axis, getReferenceUpVector());
			agent_at_axis.normalize();
			resetAxes(lerp(getAtAxis(), agent_at_axis,
					  LLCriticalDamp::getInterpolant(0.3f)));
		}

		setFocusOnAvatar();
	}

	mHUDTargetZoom = 1.f;

	dont_reenter = false;
}

// Handle any actions that need to be performed when the main app gains focus
// (such as through alt-tab).
void LLAgent::onAppFocusGained()
{
//MK
	if (gRLenabled)
	{
		return;
	}
//mk
	if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		changeCameraToDefault();
		gToolMgr.clearSavedTool();
	}
}

void LLAgent::ageChat()
{
	if (isAgentAvatarValid())
	{
		// Get amount of time since I last chatted
		F64 elapsed_time = (F64)gAgentAvatarp->mChatTimer.getElapsedTimeF32();
		// Add in frame time * 3 (so it ages 4x)
		gAgentAvatarp->mChatTimer.setAge(elapsed_time +
										 (F64)gFrameDT *
										 (CHAT_AGE_FAST_RATE - 1.0));
	}
}

// Allow camera to be moved somewhere other than behind avatar.
void LLAgent::unlockView()
{
//MK
	if (gRLenabled &&
		(gRLInterface.contains("camunlock") ||
		 gRLInterface.contains("setcam_unlock")))
	{
		return;
	}
//mk
	if (getFocusOnAvatar())
	{
		if (isAgentAvatarValid())
		{
			setFocusGlobal(LLVector3d::zero, gAgentAvatarp->mID);
		}
		setFocusOnAvatar(false, false);	// No animation
	}
}

void LLAgent::moveAt(S32 direction, bool reset)
{
	// Age chat timer so it fades more quickly when you are intentionally
	// moving
	ageChat();

	setKey(direction, mAtKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_AT_POS | AGENT_CONTROL_FAST_AT);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_AT_NEG | AGENT_CONTROL_FAST_AT);
	}

	if (reset)
	{
		resetView();
	}
}

void LLAgent::moveAtNudge(S32 direction)
{
	// Age chat timer so it fades quicker when you are intentionally moving
	ageChat();

	setKey(direction, mWalkKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_AT_POS);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_AT_NEG);
	}

	resetView();
}

void LLAgent::moveLeft(S32 direction)
{
	// Age chat timer so it fades quicker when you are intentionally moving
	ageChat();

	setKey(direction, mLeftKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_LEFT_POS | AGENT_CONTROL_FAST_LEFT);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_LEFT_NEG | AGENT_CONTROL_FAST_LEFT);
	}

	resetView();
}

void LLAgent::moveLeftNudge(S32 direction)
{
	// Age chat timer so it fades quicker when you are intentionally moving
	ageChat();

	setKey(direction, mLeftKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_LEFT_POS);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_LEFT_NEG);
	}

	resetView();
}

void LLAgent::moveUp(S32 direction)
{
	// Age chat timer so it fades quicker when you are intentionally moving
	ageChat();

	setKey(direction, mUpKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_UP_POS | AGENT_CONTROL_FAST_UP);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_UP_NEG | AGENT_CONTROL_FAST_UP);
	}

	resetView();
}

void LLAgent::moveYaw(F32 mag, bool reset_view)
{
	mYawKey = mag;

	if (mag > 0.f)
	{
		setControlFlags(AGENT_CONTROL_YAW_POS);
	}
	else if (mag < 0.f)
	{
		setControlFlags(AGENT_CONTROL_YAW_NEG);
	}

	if (reset_view)
	{
		resetView();
	}
}

void LLAgent::movePitch(S32 direction)
{
	setKey(direction, mPitchKey);

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_PITCH_POS);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_PITCH_NEG);
	}
}

// Does this parcel allow you to fly ?
bool LLAgent::canFly()
{
//MK
	if (gRLenabled && gRLInterface.mContainsFly)
	{
		return false;
	}
//mk
	if (isGodlike())
	{
		return true;
	}

	if (mRegionp && mRegionp->getBlockFly())
	{
		return false;
	}

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel)
	{
		return false;
	}

	return parcel->getAllowFly() ||
		   // Allow owners to fly on their own land.
		   LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_ALLOW_FLY);
}

void LLAgent::setFlying(bool fly, bool play_failed_sound)
{
	if (fly)
	{
		if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
		{
			// Do not allow taking off while sitting
			return;
		}
//MK
		if (gRLenabled && gRLInterface.mContainsFly)
		{
			return;
		}
//mk
		bool was_flying = getFlying();
		if (!was_flying && !canFly())
		{
			// Parcel does not let you start fly, gods can always fly and it is
			// OK if you are already flying
			if (play_failed_sound)
			{
				make_ui_sound("UISndBadKeystroke");
			}
			return;
		}
		if (!was_flying)
		{
			gViewerStats.incStat(LLViewerStats::ST_FLY_COUNT);
		}
		setControlFlags(AGENT_CONTROL_FLY);
		gSavedSettings.setBool("FlyBtnState", true);
	}
	else
	{
		clearControlFlags(AGENT_CONTROL_FLY);
		gSavedSettings.setBool("FlyBtnState", false);
	}
	mFlagsDirty = true;
}

// UI based mechanism of setting fly state
void LLAgent::toggleFlying()
{
	bool fly = !(mControlFlags & AGENT_CONTROL_FLY);
	setFlying(fly);
	resetView();
}

boost::signals2::connection LLAgent::addRegionChangedCB(const region_change_cb_t::slot_type& cb)
{
	return mRegionChangeSignal.connect(cb);
}

// Deals with baked textures uploads in OpenSIM (limit them to BAKED_HAIR
// unless the region advertizes bake on mesh support (actually Universal
// additional/mesh-only bakes) and the user accepts breaking their avatar
// look for users around them using older viewers).
bool LLAgent::setUploadedBakesLimit()
{
	static LLCachedControl<bool> os_bom(gSavedSettings,
										"OSAllowBakeOnMeshUploads");

	U8 old_num_bakes = mUploadedBakes;
	if ((gIsInSecondLife || os_bom) &&
		mRegionp && mRegionp->bakesOnMeshEnabled())
	{
		mUploadedBakes = BAKED_NUM_INDICES;
	}
	else
	{
		mUploadedBakes = BAKED_HAIR + 1;
	}

	if (!gIsInSecondLife && mUploadedBakes != old_num_bakes &&
		 isAgentAvatarValid())
	{
		llinfos << "Detected change in uploaded bakes number, scheduling a rebake..."
				<< llendl;
		mRebakeNeeded = false;
		doAfterInterval(boost::bind(&LLVOAvatarSelf::forceBakeAllTextures,
									gAgentAvatarp.get(), true), 5.f);
		return true;
	}

	return false;
}

void LLAgent::handleServerFeaturesTransition()
{
	if (!mRegionp)
	{
		mHasExtEnvironment = mInventorySettings = mInventoryMaterial = false;
		return;
	}

	// Some capabilities must be passed to library classes for the agent
	// region.
	const std::string& cap1 = mRegionp->getCapability("GetDisplayNames");
	LLAvatarNameCache::setNameLookupURL(cap1);
	llinfos << "Avatar names lookup URL set to: "
			<< (cap1.empty() ? "none" : cap1) << llendl;

	// Make sure the name tags will be refreshed, using the (possibly new)
	// avatar name cache capability.
	LLVOAvatar::invalidateNameTags();

	const std::string& cap2 = mRegionp->getCapability("GetExperienceInfo");
	LLExperienceCache::setLookupURL(cap2);
	llinfos << "Experiences lookup URL set to: "
			<< (cap2.empty() ? "none" : cap2) << llendl;

	LLPuppetMotion::requestPuppetryStatus(mRegionp);

	mHasExtEnvironment = hasRegionCapability("ExtEnvironment");
	mInventorySettings = hasRegionCapability("UpdateSettingsTaskInventory") &&
						 hasRegionCapability("UpdateSettingsAgentInventory");
	mInventoryMaterial = hasRegionCapability("UpdateMaterialTaskInventory") &&
						 hasRegionCapability("UpdateMaterialAgentInventory");

#if 0	// Not needed any more since now the whole grid should have it (SL) or
		// not at all (OpenSim).
	// See if the Marketplace Listings folder is supported in this region
	LLMarketplace::checkMerchantStatus();
#endif

	// NOTE: the avatar is not yet fully rezzed when logging in and the
	// capabilities are received and trigger a first call to this method...
	if (!isAgentAvatarValid()) return;

	// Make sure to use the proper method to account for the Z-Offset: using
	// the new Avatar Hover Offset capability/feature if available or, in
	// non-SSB sims, as a simple offset added to the size sent by
	// sendAgentSetAppearance().
	gAgentAvatarp->scheduleHoverUpdate();

	// Deal with baked textures uploads in OpenSim
	if (setUploadedBakesLimit())
	{
		// If a rebake has been scheduled, skip the rest...
		return;
	}

	// We needed a rebake just after the region capabilities were received, so
	// we can do it now.
	if (mRebakeNeeded)
	{
		mRebakeNeeded = false;
		gAppearanceMgr.incrementCofVersion();
		gAppearanceMgr.resetCOFUpdateTimer();
		return;
	}

	// SSB transition: not needed any more in SL, but I kept it in case OpenSim
	// would implement SSB, sometime in the future. HB
	bool server_baked = gAgentAvatarp->isUsingServerBakes();
	if (LLVOAvatarSelf::canUseServerBaking())
	{
		if (!server_baked)
		{
			// Old-style appearance entering a server-bake region.
			llinfos << "Rebake requested due to region transition" << llendl;
			gAppearanceMgr.requestServerAppearanceUpdate();
		}
	}
	else if (server_baked)
	{
		// New-style appearance entering a non-bake region: force a rebake.
		// Trying to rebake immediately after crossing region boundary seems to
		// be failure prone; adding a delay factor. Yes, this fix is ad-hoc and
		// not guaranteed to work in all cases.
		gAppearanceMgr.setRebaking();
		doAfterInterval(boost::bind(&LLVOAvatarSelf::forceBakeAllTextures,
									gAgentAvatarp.get(), true), 5.f);
		llinfos << "Rebake requested due to region transition" << llendl;
	}
}

void LLAgent::setRegion(LLViewerRegion* regionp)
{
	if (regionp && mRegionp != regionp)
	{
//MK
		if (!gRLenabled || !gRLInterface.mContainsShowloc)
//mk
		{
			llinfos << "Moving agent into region: " << regionp->getIdentity()
					<< llendl;
		}

		// Clear all ban lines
		gViewerParcelMgr.resetCollisionSegments();

		// We have changed region and we are now going to change our agent
		// coordinate frame.
		mAgentOriginGlobal = regionp->getOriginGlobal();
		LLViewerCamera* camera = &gViewerCamera;
		LLVector3 camera_position_agent = camera->getOrigin();
		LLVector3 delta;
		LLVector3d agent_offset_global;
		if (mRegionp)
		{
			// Force the interest list mode back to "default" for the region we
			// are leaving... HB
			mRegionp->setInterestListMode(true);

			// Set departure and arrival handle, used to detect far TPs. HB
			mDepartureHandle = mRegionp->getHandle();
			mArrivalHandle = regionp->getHandle();
			LL_DEBUGS("Teleport") << "Set departure handle to "
								  << mDepartureHandle
								  << ", and arrival handle to "
								  << mArrivalHandle << LL_ENDL;
			// Start afresh for textures loading in the new place. HB
			if (mArrivalHandle != mDepartureHandle)
			{
				LLViewerTexture::resetLowMemCondition();
			}

			agent_offset_global = mRegionp->getOriginGlobal();
			delta.set(regionp->getOriginGlobal() - agent_offset_global);

			// Hack to keep sky in the agent's region, otherwise it may get
			// deleted - DJS 08/02/02
			// *TODO: possibly refactor into gSky->setAgentRegion(regionp) ?
			if (gSky.mVOSkyp)
			{
				gSky.mVOSkyp->setRegion(regionp);
			}
		}
		else
		{
			// First time initialization.
			LLViewerTexture::resetLowMemCondition();
			agent_offset_global = mAgentOriginGlobal;
			delta.set(agent_offset_global);
		}

		setPositionAgent(getPositionAgent() - delta);
		camera->setOrigin(camera_position_agent - delta);

		// When automatic stale GL textures cleanup is disabled, do clean them
		// up once after each arrival in a new simulator. HB
		if (gSavedSettings.getU32("StaleGLImageCleanupMinDelay") == 0)
		{
			LLImageGL::activateStaleTextures();
		}
	}
	else if (regionp && regionp == mRegionp)
	{
		llinfos << "Region unchanged" << llendl;
		mDepartureHandle = mArrivalHandle = regionp->getHandle();
		LL_DEBUGS("Teleport") << "Departure and arrival handle set to "
							  << mArrivalHandle << LL_ENDL;
	}
	else if (mRegionp && !regionp && !LLApp::isQuitting())
	{
		llwarns << "Setting agent region to NULL." << llendl;
	}

	mRegionp = regionp;

	if (regionp)
	{
		regionp->setInterestListMode();
		// Must shift hole-covering water object locations because local
		// coordinate frame changed.
		gWorld.updateWaterObjects();

		// Keep a list of regions we have been to. This is just an interesting
		// stat, logged at the dataserver we could trake this at the dataserver
		// side, but this is harder.
		U64 handle = regionp->getHandle();
		mRegionsVisited.insert(handle);
		if (mDepartureHandle == 0)	// If never initialized
		{
			LL_DEBUGS("Teleport") << "Set departure handle to: " << handle
								  << LL_ENDL;
			mDepartureHandle = handle;
		}

		gSelectMgr.updateSelectionCenter();

		// Let interested parties know agent region has been changed.
		mRegionChangeSignal();
		LLHUDEffectLookAt::updateSettings();

		// Check for transitional features changes between regions
		if (regionp->capabilitiesReceived())
		{
			handleServerFeaturesTransition();
		}
		else
		{
			// Need to handle via callback after caps arrive.
			regionp->setCapsReceivedCB(boost::bind(&LLAgent::handleServerFeaturesTransition,
												   this));
		}

		// *HACK: make sure all objects get rezzed in the region of arrival. HB
		U32 sim_change_type = mTeleportState == TELEPORT_NONE ? 2 : 4;
		schedule_objects_visibility_refresh(sim_change_type);
	}
}

U64 LLAgent::getRegionHandle() const
{
	return mRegionp ? mRegionp->getHandle() : 0;
}

const LLHost& LLAgent::getRegionHost() const
{
	return mRegionp ? mRegionp->getHost() : LLHost::invalid;
}

// Returns empty() if mRegionp == NULL
std::string LLAgent::getSLURL() const
{
	if (mRegionp)
	{
		LLVector3 pos = ((LLAgent*)this)->getPositionAgent();
		LLSLURL slurl(mRegionp->getName(), pos);
		return slurl.getSLURLString();
	}
	return LLStringUtil::null;
}

const std::string& LLAgent::getRegionCapability(const char* cap)
{
	return mRegionp ? mRegionp->getCapability(cap) : LLStringUtil::null;
}

bool LLAgent::regionCapabilitiesReceived() const
{
	return mRegionp && mRegionp->capabilitiesReceived();
}

bool LLAgent::hasRegionCapability(const char* cap) const
{
	return mRegionp && !mRegionp->getCapability(cap).empty();
}

bool LLAgent::regionHasExportPermSupport() const
{
	return mRegionp && mRegionp->isOSExportPermSupported();
}

bool LLAgent::inPrelude()
{
	return mRegionp && mRegionp->isPrelude();
}

bool LLAgent::canManageEstate() const
{
	return mRegionp && mRegionp->canManageEstate();
}

void LLAgent::sendMessage()
{
	if (gDisconnected)
	{
		llwarns << "Trying to send message when disconnected !" << llendl;
		return;
	}
	if (!mRegionp)
	{
		llwarns << "No region for agent yet !" << llendl;
		llassert(false);
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		msg->sendMessage(mRegionp->getHost());
	}
	else
	{
		llwarns << "Message system pointer is NULL !" << llendl;
	}
}

void LLAgent::sendReliableMessage(U32 retries_factor)
{
	if (gDisconnected)
	{
		LL_DEBUGS("Agent") << "Trying to send message when disconnected !"
						   << LL_ENDL;
		return;
	}
	if (!mRegionp)
	{
		LL_DEBUGS("Agent") << "No region for agent yet, not sending message !"
						   << LL_ENDL;
		return;
	}
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)
	{
		llwarns << "Message system pointer is NULL !" << llendl;
		return;
	}
	msg->sendReliable(mRegionp->getHost(), retries_factor);
}

LLVector3 LLAgent::getVelocity() const
{
	if (!isAgentAvatarValid())
	{
		return LLVector3::zero;
	}
	return gAgentAvatarp->getVelocity();
}

void LLAgent::setPositionAgent(const LLVector3& pos_agent)
{
	if (!pos_agent.isFinite())
	{
		llwarns << "Got an infinite position. Ignoring." << llendl;
		llassert(false);
		return;
	}

	LLViewerObject* parentp = NULL;
	if (isAgentAvatarValid())
	{
		parentp = (LLViewerObject*)gAgentAvatarp->getParent();
	}
	if (parentp)
	{
		LLVector3 pos_agent_sitting;
		LLVector3d pos_agent_d;
		pos_agent_sitting = gAgentAvatarp->getPosition() *
							parentp->getRotation() +
							parentp->getPositionAgent();
		pos_agent_d.set(pos_agent_sitting);

		mFrameAgent.setOrigin(pos_agent_sitting);
		mPositionGlobal = pos_agent_d + mAgentOriginGlobal;
	}
	else
	{
		mFrameAgent.setOrigin(pos_agent);

		LLVector3d pos_agent_d;
		pos_agent_d.set(pos_agent);
		mPositionGlobal = pos_agent_d + mAgentOriginGlobal;
	}
}

void LLAgent::slamLookAt(const LLVector3& look_at)
{
	LLVector3 look_at_norm = look_at;
	look_at_norm.mV[VZ] = 0.f;
	look_at_norm.normalize();
	resetAxes(look_at_norm);
}

boost::signals2::connection LLAgent::setPosChangeCallback(const pos_change_signal_t::slot_type& cb)
{
	return mPosChangeSignal.connect(cb);
}

const LLVector3d& LLAgent::getPositionGlobal() const
{
	if (isAgentAvatarValid() && gAgentAvatarp->mDrawable.notNull())
	{
		mPositionGlobal =
			getPosGlobalFromAgent(gAgentAvatarp->getRenderPosition());
	}
	else
	{
		mPositionGlobal = getPosGlobalFromAgent(mFrameAgent.getOrigin());
	}

	return mPositionGlobal;
}

const LLVector3& LLAgent::getPositionAgent()
{
	if (isAgentAvatarValid())
	{
		if (gAgentAvatarp->mDrawable.notNull())
		{
			mFrameAgent.setOrigin(gAgentAvatarp->getPositionAgent());
		}
		else
		{
			mFrameAgent.setOrigin(gAgentAvatarp->getRenderPosition());
		}
	}

	return mFrameAgent.getOrigin();
}

S32 LLAgent::getRegionsVisited() const
{
	return mRegionsVisited.size();
}

LLVector3 LLAgent::getPosAgentFromGlobal(const LLVector3d &pos_global) const
{
	LLVector3 pos_agent;
	pos_agent.set(pos_global - mAgentOriginGlobal);
	return pos_agent;
}

LLVector3d LLAgent::getPosGlobalFromAgent(const LLVector3& pos_agent) const
{
	LLVector3d pos_agent_d;
	pos_agent_d.set(pos_agent);
	return pos_agent_d + mAgentOriginGlobal;
}

void LLAgent::resetAxes()
{
	mFrameAgent.resetAxes();
}

// Copied from LLCamera::setOriginAndLookAt; look_at must be unit vector.
void LLAgent::resetAxes(const LLVector3& look_at)
{
	LLVector3 skyward = getReferenceUpVector();

	// If look_at has zero length or if look_at and skyward are parallel, fail.
	// Test both of these conditions with a cross product.
	LLVector3 cross(look_at % skyward);
	if (cross.isNull())
	{
		LL_DEBUGS("Agent") << "Cross-product is zero. Skipped." << LL_ENDL;
		return;
	}

	// Make sure look_at and skyward are not parallel and neither are 0 length
	LLVector3 left(skyward % look_at);
	LLVector3 up(look_at % left);

	mFrameAgent.setAxes(look_at, left, up);
}

void LLAgent::rotate(F32 angle, const LLVector3& axis)
{
	mFrameAgent.rotate(angle, axis);
}

void LLAgent::rotate(F32 angle, F32 x, F32 y, F32 z)
{
	mFrameAgent.rotate(angle, x, y, z);
}

void LLAgent::rotate(const LLMatrix3& matrix)
{
	mFrameAgent.rotate(matrix);
}

void LLAgent::rotate(const LLQuaternion& quaternion)
{
	mFrameAgent.rotate(quaternion);
}

// Returns vector is in the coordinate frame of the avatar's parent object, or
// the world if none
LLVector3 LLAgent::getReferenceUpVector()
{
	LLVector3 up_vector = LLVector3::z_axis;

	LLViewerObject* parentp = NULL;
	if (isAgentAvatarValid() && gAgentAvatarp->mDrawable.notNull())
	{
		parentp = (LLViewerObject*)gAgentAvatarp->getParent();
	}
	if (parentp)
	{
		U32 camera_mode = mCameraAnimating ? mLastCameraMode : mCameraMode;
		// And in third person...
		if (camera_mode == CAMERA_MODE_THIRD_PERSON)
		{
			// Make the up vector point to the absolute +z axis
			up_vector = up_vector * ~(parentp->getRenderRotation());
		}
		else if (camera_mode == CAMERA_MODE_MOUSELOOK)
		{
			// Make the up vector point to the avatar's +z axis
			up_vector = up_vector * gAgentAvatarp->mDrawable->getRotation();
		}
	}

	return up_vector;
}

// Radians, positive is forward into ground
void LLAgent::pitch(F32 angle)
{
	// Do not let the user pitch if pointed almost all the way down or up
	mFrameAgent.pitch(clampPitchToLimits(angle));
}

// Radians, positive is forward into ground
F32 LLAgent::clampPitchToLimits(F32 angle)
{
	// A dot B = mag(A) * mag(B) * cosf(angle between A and B)
	// so... cosf(angle between A and B) = A dot B / mag(A) / mag(B)
	//									 = A dot B for unit vectors

	LLVector3 skyward = getReferenceUpVector();

	F32 look_down_limit;
	F32 look_up_limit = 10.f * DEG_TO_RAD;

	F32 angle_from_skyward = acosf(mFrameAgent.getAtAxis() * skyward);

	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{
		look_down_limit = 130.f * DEG_TO_RAD;
	}
	else
	{
		look_down_limit = 170.f * DEG_TO_RAD;
	}

	// Clamp pitch to limits
	if (angle >= 0.f && angle_from_skyward + angle > look_down_limit)
	{
		angle = look_down_limit - angle_from_skyward;
	}
	else if (angle < 0.f && angle_from_skyward + angle < look_up_limit)
	{
		angle = look_up_limit - angle_from_skyward;
	}

	return angle;
}

void LLAgent::roll(F32 angle)
{
	mFrameAgent.roll(angle);
}

void LLAgent::yaw(F32 angle)
{
	if (!rotateGrabbed())
	{
		mFrameAgent.rotate(angle, getReferenceUpVector());
	}
}

bool LLAgent::noCameraConstraints()
{
	static LLCachedControl<bool> no_constraints(gSavedSettings,
												"DisableCameraConstraints");
	return no_constraints
//MK
		   && !(gRLenabled &&
		   		(gRLInterface.mCamDistMax < EXTREMUM ||
				 gRLInterface.mCamDistMin > -EXTREMUM ||
				 gRLInterface.mCamZoomMax < EXTREMUM ||
				 gRLInterface.mCamZoomMin > -EXTREMUM));
//mk
}

LLVector3 LLAgent::calcFocusOffset(LLViewerObject* object,
								   LLVector3 original_focus_point,
								   S32 x, S32 y)
{
	LLVector3 obj_pos = object->getRenderPosition();

	// If it is an avatar or an animesh object, do not do any funky heuristics
	// to position the focal point. See DEV-30589.
	if (!gViewerWindowp || object->isAvatar() ||
		(object->isAnimatedObject() && object->getPuppetAvatar()))
	{
		return original_focus_point - obj_pos;
	}

	const LLMatrix4& obj_matrix = object->getRenderMatrix();
	LLQuaternion obj_rot = object->getRenderRotation();
	LLViewerCamera* camera = &gViewerCamera;

	LLQuaternion inv_obj_rot = ~obj_rot; // get inverse of rotation
	LLVector3 object_extents = object->getScale();
	// Make sure they object extents are non-zero
	object_extents.clamp(0.001f, F32_MAX);

	// obj_to_cam_ray is unit vector pointing from object center to camera, in
	// the coordinate frame of the object
	LLVector3 obj_to_cam_ray = obj_pos - camera->getOrigin();
	obj_to_cam_ray.rotVec(inv_obj_rot);
	obj_to_cam_ray.normalize();

	// obj_to_cam_ray_proportions are the (positive) ratios of
	// the obj_to_cam_ray x,y,z components with the x,y,z object dimensions.
	LLVector3 obj_to_cam_ray_proportions;
	obj_to_cam_ray_proportions.mV[VX] = fabsf(obj_to_cam_ray.mV[VX] /
											  object_extents.mV[VX]);
	obj_to_cam_ray_proportions.mV[VY] = fabsf(obj_to_cam_ray.mV[VY] /
											  object_extents.mV[VY]);
	obj_to_cam_ray_proportions.mV[VZ] = fabsf(obj_to_cam_ray.mV[VZ] /
											  object_extents.mV[VZ]);

	// Find the largest ratio stored in obj_to_cam_ray_proportions.
	// This corresponds to the object's local axial plane (XY, YZ, XZ) that is
	// *most* facing the camera
	LLVector3 longest_object_axis;
	// Is x-axis longest ?
	if (obj_to_cam_ray_proportions.mV[VX] > obj_to_cam_ray_proportions.mV[VY] &&
		obj_to_cam_ray_proportions.mV[VX] > obj_to_cam_ray_proportions.mV[VZ])
	{
		// Then grab it
		longest_object_axis.set(obj_matrix.getFwdRow4());
	}
	// Is y-axis longest ?
	else if (obj_to_cam_ray_proportions.mV[VY] > obj_to_cam_ray_proportions.mV[VZ])
	{
		// Then grab it
		longest_object_axis.set(obj_matrix.getLeftRow4());
	}
	// Otherwise, use z axis
	else
	{
		longest_object_axis.set(obj_matrix.getUpRow4());
	}

	// Use this axis as the normal to project mouse click on to plane with that
	// normal, at the object center.
	// This generates a point behind the mouse cursor that is approximately in
	// the middle of the object in terms of depth.
	// We do this to allow the camera rotation tool to "tumble" the object by
	// rotating the camera.
	// If the focus point were the object surface under the mouse, camera
	// rotation would introduce an undesirable eccentricity to the object
	// orientation
	LLVector3 focus_plane_normal(longest_object_axis);
	focus_plane_normal.normalize();

	LLVector3d focus_pt_global;
	gViewerWindowp->mousePointOnPlaneGlobal(focus_pt_global, x, y,
										   getPosGlobalFromAgent(obj_pos),
										   focus_plane_normal);
	LLVector3 focus_pt = getPosAgentFromGlobal(focus_pt_global);

	// Find vector from camera to focus point in object space
	LLVector3 camera_to_focus_vec = focus_pt - camera->getOrigin();
	camera_to_focus_vec.rotVec(inv_obj_rot);

	// Find vector from object origin to focus point in object coordinates
	LLVector3 focus_offset_from_object_center = focus_pt - obj_pos;
	// convert to object-local space
	focus_offset_from_object_center.rotVec(inv_obj_rot);

	// We need to project the focus point back into the bounding box of the
	// focused object.
	// Do this by calculating the XYZ scale factors needed to get focus offset
	// back in bounds along the camera_focus axis
	LLVector3 clip_fraction;

	// For each axis...
	for (U32 axis = VX; axis <= VZ; ++axis)
	{
		// Calculate distance that focus offset sits outside of bounding box
		// along that axis. NOTE: dist_out_of_bounds keeps the sign of
		// focus_offset_from_object_center
		F32 dist_out_of_bounds;
		if (focus_offset_from_object_center.mV[axis] > 0.f)
		{
			dist_out_of_bounds = llmax(0.f,
									   focus_offset_from_object_center.mV[axis] -
									   object_extents.mV[axis] * 0.5f);
		}
		else
		{
			dist_out_of_bounds = llmin(0.f,
									   focus_offset_from_object_center.mV[axis] +
									   object_extents.mV[axis] * 0.5f);
		}

		// Then calculate the scale factor needed to push camera_to_focus_vec
		// back in bounds along current axis
		if (fabsf(camera_to_focus_vec.mV[axis]) < 0.0001f)
		{
			// don't divide by very small number
			clip_fraction.mV[axis] = 0.f;
		}
		else
		{
			clip_fraction.mV[axis] = dist_out_of_bounds / camera_to_focus_vec.mV[axis];
		}
	}

	LLVector3 abs_clip_fraction = clip_fraction;
	abs_clip_fraction.abs();

	// Find axis of focus offset that is *most* outside the bounding box and
	// use that to rescale focus offset to inside object extents
	if (abs_clip_fraction.mV[VX] > abs_clip_fraction.mV[VY] &&
		abs_clip_fraction.mV[VX] > abs_clip_fraction.mV[VZ])
	{
		focus_offset_from_object_center -= clip_fraction.mV[VX] * camera_to_focus_vec;
	}
	else if (abs_clip_fraction.mV[VY] > abs_clip_fraction.mV[VZ])
	{
		focus_offset_from_object_center -= clip_fraction.mV[VY] * camera_to_focus_vec;
	}
	else
	{
		focus_offset_from_object_center -= clip_fraction.mV[VZ] * camera_to_focus_vec;
	}

	// Convert back to world space
	focus_offset_from_object_center.rotVec(obj_rot);

	// Now, based on distance of camera from object relative to object size
	// push the focus point towards the near surface of the object when
	// (relatively) close to the object or keep the focus point in the object
	// middle when (relatively) far.
	// NOTE: leave focus point in middle of avatars, since the behavior you
	// want when alt-zooming on avatars is almost always "tumble about middle"
	// and not "spin around surface point"
	LLVector3 obj_rel = original_focus_point - object->getRenderPosition();

	// Now that we have the object relative position, we should bias toward the
	// center of the object based on the distance of the camera to the focus
	// point vs. the distance of the camera to the focus.
	F32 relDist = fabsf(obj_rel * camera->getAtAxis());
	F32 viewDist = dist_vec(obj_pos + obj_rel, camera->getOrigin());

	LLBBox obj_bbox = object->getBoundingBoxAgent();
	F32 bias = 0.f;

	// virtual_camera_pos is the camera position we are simulating by backing
	// the camera off and adjusting the FOV
	LLVector3 virtual_camera_pos;
	virtual_camera_pos = getPosAgentFromGlobal(mFocusTargetGlobal +
											   (getCameraPositionGlobal() -
												mFocusTargetGlobal) /
											   (1.f + mCameraFOVZoomFactor));

	// If the camera is inside the object (large, hollow objects, for example)
	// leave focus point all the way to destination depth, away from object
	// center
	if (!obj_bbox.containsPointAgent(virtual_camera_pos))
	{
		// Perform magic number biasing of focus point towards surface versus
		// planar center
		bias = clamp_rescale(relDist/viewDist, 0.1f, 0.7f, 0.f, 1.f);
		obj_rel = lerp(focus_offset_from_object_center, obj_rel, bias);
	}

	return obj_rel;
}

bool LLAgent::calcCameraMinDistance(F32& obj_min_distance)
{
	if (noCameraConstraints() ||
		!mFocusObject || mFocusObject->isDead() || mFocusObject->isMesh())
	{
		obj_min_distance = 0.f;
		return true;
	}

	// Tells whether the bounding box is to be treated literally (volumes) or
	// as an approximation (avatars)
	bool soft_limit = false;

	if (mFocusObject->mDrawable.isNull())
	{
		llwarns << "Focus object with no drawable !" << llendl;
#if LL_DEBUG
		mFocusObject->dump();
		llassert(false);
#endif
		obj_min_distance = 0.f;
		return true;
	}

	LLQuaternion inv_object_rot = ~mFocusObject->getRenderRotation();
	LLVector3 target_offset_origin = mFocusObjectOffset;
	LLVector3 camera_offset_target(getCameraPositionAgent() -
								   getPosAgentFromGlobal(mFocusTargetGlobal));

	// convert offsets into object local space
	camera_offset_target.rotVec(inv_object_rot);
	target_offset_origin.rotVec(inv_object_rot);

	// push around object extents based on target offset
	LLVector3 object_extents = mFocusObject->getScale();
	if (mFocusObject->isAvatar())
	{
		// Fudge factors that lets you zoom in on avatars a bit more (which
		// do not do FOV zoom)
		object_extents.mV[VX] *= AVATAR_ZOOM_MIN_X_FACTOR;
		object_extents.mV[VY] *= AVATAR_ZOOM_MIN_Y_FACTOR;
		object_extents.mV[VZ] *= AVATAR_ZOOM_MIN_Z_FACTOR;
		soft_limit = true;
	}
	LLVector3 abs_target_offset = target_offset_origin;
	abs_target_offset.abs();

	LLVector3 target_offset_dir = target_offset_origin;

	bool target_outside_object_extents = false;
	for (U32 i = VX; i <= VZ; ++i)
	{
		if (abs_target_offset.mV[i] * 2.f >
				object_extents.mV[i] + OBJECT_EXTENTS_PADDING)
		{
			target_outside_object_extents = true;
		}
		if (camera_offset_target.mV[i] > 0.f)
		{
			object_extents.mV[i] -= target_offset_origin.mV[i] * 2.f;
		}
		else
		{
			object_extents.mV[i] += target_offset_origin.mV[i] * 2.f;
		}
	}

	// Do not shrink the object extents so far that the object inverts
	object_extents.clamp(0.001f, F32_MAX);

	// Move into first octant
	LLVector3 camera_offset_target_abs_norm = camera_offset_target;
	camera_offset_target_abs_norm.abs();
	// make sure offset is non-zero
	camera_offset_target_abs_norm.clamp(0.001f, F32_MAX);
	camera_offset_target_abs_norm.normalize();

	// Find camera position relative to normalized object extents
	LLVector3 camera_offset_target_scaled = camera_offset_target_abs_norm;
	camera_offset_target_scaled.mV[VX] /= object_extents.mV[VX];
	camera_offset_target_scaled.mV[VY] /= object_extents.mV[VY];
	camera_offset_target_scaled.mV[VZ] /= object_extents.mV[VZ];

	if (camera_offset_target_scaled.mV[VX] > camera_offset_target_scaled.mV[VY] &&
		camera_offset_target_scaled.mV[VX] > camera_offset_target_scaled.mV[VZ])
	{
		if (camera_offset_target_abs_norm.mV[VX] < 0.001f)
		{
			obj_min_distance = object_extents.mV[VX] * 0.5f;
		}
		else
		{
			obj_min_distance = object_extents.mV[VX] *
							   0.5f / camera_offset_target_abs_norm.mV[VX];
		}
	}
	else if (camera_offset_target_scaled.mV[VY] > camera_offset_target_scaled.mV[VZ])
	{
		if (camera_offset_target_abs_norm.mV[VY] < 0.001f)
		{
			obj_min_distance = object_extents.mV[VY] * 0.5f;
		}
		else
		{
			obj_min_distance = object_extents.mV[VY] *
							   0.5f / camera_offset_target_abs_norm.mV[VY];
		}
	}
	else
	{
		if (camera_offset_target_abs_norm.mV[VZ] < 0.001f)
		{
			obj_min_distance = object_extents.mV[VZ] * 0.5f;
		}
		else
		{
			obj_min_distance = object_extents.mV[VZ] *
							   0.5f / camera_offset_target_abs_norm.mV[VZ];
		}
	}

	LLVector3 object_split_axis;
	LLVector3 target_offset_scaled = target_offset_origin;
	target_offset_scaled.abs();
	target_offset_scaled.normalize();
	target_offset_scaled.mV[VX] /= object_extents.mV[VX];
	target_offset_scaled.mV[VY] /= object_extents.mV[VY];
	target_offset_scaled.mV[VZ] /= object_extents.mV[VZ];

	if (target_offset_scaled.mV[VX] > target_offset_scaled.mV[VY] &&
		target_offset_scaled.mV[VX] > target_offset_scaled.mV[VZ])
	{
		object_split_axis = LLVector3::x_axis;
	}
	else if (target_offset_scaled.mV[VY] > target_offset_scaled.mV[VZ])
	{
		object_split_axis = LLVector3::y_axis;
	}
	else
	{
		object_split_axis = LLVector3::z_axis;
	}

	LLVector3 camera_offset_object(getCameraPositionAgent() -
								   mFocusObject->getPositionAgent());

	F32 camera_offset_clip = camera_offset_object * object_split_axis;
	F32 target_offset_clip = target_offset_dir * object_split_axis;

	// Target has moved outside of object extents. Check to see if camera and
	// target are on same side.
	if (target_outside_object_extents &&
		((camera_offset_clip > 0.f && target_offset_clip > 0.f) ||
		 (camera_offset_clip < 0.f && target_offset_clip < 0.f)))
	{
		return false;
	}

	// Clamp obj distance to diagonal of 10 by 10 cube
	obj_min_distance = llmin(obj_min_distance, 10.f * F_SQRT3);

	obj_min_distance += gViewerCamera.getNear() + (soft_limit ? 0.1f : 0.2f);

	return true;
}

F32 LLAgent::getCameraZoomFraction()
{
	// 0.f: camera zoomed all the way out; 1.f: camera zoomed all the way in
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() &&
		selection->getSelectType() == SELECT_TYPE_HUD)
	{
		// Already [0,1]
		return mHUDTargetZoom;
	}

	if (mFocusOnAvatar && cameraThirdPerson())
	{
		return clamp_rescale(mCameraZoomFraction, MIN_ZOOM_FRACTION,
							 MAX_ZOOM_FRACTION, 1.f, 0.f);
	}

	if (cameraCustomizeAvatar())
	{
		F32 distance = (F32)mCameraFocusOffsetTarget.length();
		return clamp_rescale(distance, APPEARANCE_MIN_ZOOM,
							 APPEARANCE_MAX_ZOOM, 1.f, 0.f);
	}

	F32 distance = (F32)mCameraFocusOffsetTarget.length();

	constexpr F32 DIST_FUDGE = 16.f; // In meters
	F32 region_with = mRegionp ? mRegionp->getWidth() : REGION_WIDTH_METERS;
	F32 max_zoom = llmin(mDrawDistance - DIST_FUDGE, region_with - DIST_FUDGE,
						 MAX_CAMERA_DISTANCE_FROM_AGENT);
	F32 min_zoom;
	if (mFocusObject.notNull())
	{
		if (mFocusObject->isAvatar())
		{
			min_zoom = AVATAR_MIN_ZOOM;
		}
		else
		{
			min_zoom = OBJECT_MIN_ZOOM;
		}
	}
	else
	{
		min_zoom = LAND_MIN_ZOOM;
	}
	return clamp_rescale(distance, min_zoom, max_zoom, 1.f, 0.f);
}

// fraction == 0.f for camera zoomed all the way out, 1.f for camera zoomed all
// the way in
void LLAgent::setCameraZoomFraction(F32 fraction)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() &&
		selection->getSelectType() == SELECT_TYPE_HUD)
	{
		mHUDTargetZoom = fraction;
		// Clamp target zoom level to reasonable values
//MK
		if (gRLenabled && gRLInterface.mHasLockedHuds)
		{
			mHUDTargetZoom = llclamp(mHUDTargetZoom, 0.85f, 1.f);
		}
		else
//mk
		{
			mHUDTargetZoom = llclamp(mHUDTargetZoom, 0.1f, 1.f);
		}
	}
	else if (mFocusOnAvatar && cameraThirdPerson())
	{
		mCameraZoomFraction = rescale(fraction, 0.f, 1.f,
									  MAX_ZOOM_FRACTION, MIN_ZOOM_FRACTION);
	}
	else if (cameraCustomizeAvatar())
	{
		LLVector3d camera_offset_dir = mCameraFocusOffsetTarget;
		camera_offset_dir.normalize();
		mCameraFocusOffsetTarget = camera_offset_dir *
								   rescale(fraction, 0.f, 1.f,
										   APPEARANCE_MAX_ZOOM,
										   APPEARANCE_MIN_ZOOM);
	}
	else
	{
		constexpr F32 DIST_FUDGE = 16.f; // meters
		F32 region_with = mRegionp ? mRegionp->getWidth()
								   : REGION_WIDTH_METERS;
		F32 max_zoom = llmin(mDrawDistance - DIST_FUDGE,
							 region_with - DIST_FUDGE,
							 MAX_CAMERA_DISTANCE_FROM_AGENT);

		F32 min_zoom = LAND_MIN_ZOOM;
		if (noCameraConstraints())
		{
			min_zoom = 0.f;
		}
		else if (mFocusObject.notNull())
		{
			if (mFocusObject->isAvatar())
			{
				min_zoom = AVATAR_MIN_ZOOM;
			}
			else
			{
				min_zoom = OBJECT_MIN_ZOOM;
			}
		}

		LLVector3d camera_offset_dir = mCameraFocusOffsetTarget;
		camera_offset_dir.normalize();
		mCameraFocusOffsetTarget = camera_offset_dir *
								   rescale(fraction, 0.f, 1.f, max_zoom,
										   min_zoom);
	}
	startCameraAnimation();
}

void LLAgent::cameraOrbitAround(F32 radians)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() &&
		selection->getSelectType() == SELECT_TYPE_HUD)
	{
		// Do nothing for hud selection
		return;
	}
	if (mFocusOnAvatar &&
		(mCameraMode == CAMERA_MODE_THIRD_PERSON ||
		 mCameraMode == CAMERA_MODE_FOLLOW))
	{
		mFrameAgent.rotate(radians, getReferenceUpVector());
	}
	else
	{
		mCameraFocusOffsetTarget.rotVec(radians, 0.f, 0.f, 1.f);
		cameraZoomIn(1.f);
	}
}

void LLAgent::cameraOrbitOver(F32 angle)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() &&
		selection->getSelectType() == SELECT_TYPE_HUD)
	{
		// Do nothing for hud selection
	}
	else if (mFocusOnAvatar && mCameraMode == CAMERA_MODE_THIRD_PERSON)
	{
		pitch(angle);
	}
	else
	{
		LLVector3 camera_offset_unit(mCameraFocusOffsetTarget);
		camera_offset_unit.normalize();

		F32 angle_from_up = acosf(camera_offset_unit * getReferenceUpVector());

		LLVector3d left_axis;
		left_axis.set(gViewerCamera.getLeftAxis());
		F32 new_angle = llclamp(angle_from_up - angle, 1.f * DEG_TO_RAD,
								179.f * DEG_TO_RAD);
		mCameraFocusOffsetTarget.rotVec(angle_from_up - new_angle, left_axis);

		cameraZoomIn(1.f);
	}
}

void LLAgent::cameraZoomIn(F32 fraction)
{
	if (gDisconnected)
	{
		return;
	}

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() &&
		selection->getSelectType() == SELECT_TYPE_HUD &&
		gToolMgr.inBuildMode())
	{
		// Just update hud zoom level
		mHUDTargetZoom /= fraction;
		return;
	}

	LLVector3d camera_offset_unit(mCameraFocusOffsetTarget);
	F32 min_zoom = LAND_MIN_ZOOM;
	F32 current_distance = (F32)camera_offset_unit.normalize();
	F32 new_distance = current_distance * fraction;

	bool camera_constraints = !noCameraConstraints();
	if (camera_constraints)
	{
		// Do not move through focus point
		if (mFocusObject)
		{
			min_zoom = OBJECT_MIN_ZOOM;
			if (mFocusObject->isAvatar())
			{
				calcCameraMinDistance(min_zoom);
			}
		}

		new_distance = llmax(new_distance, min_zoom);
	}

	// Do not zoom too far back
	F32 max_distance;
	if (camera_constraints)
	{
		F32 region_with = mRegionp ? mRegionp->getWidth()
								   : REGION_WIDTH_METERS;
		max_distance = llmin(mDrawDistance, region_with);
	}
	else
	{
		max_distance = 4096.f;
	}
	if (new_distance > max_distance)
	{
		new_distance = max_distance;
	}

	if (cameraCustomizeAvatar())
	{
		new_distance = llclamp(new_distance, APPEARANCE_MIN_ZOOM,
							   APPEARANCE_MAX_ZOOM);
	}

	mCameraFocusOffsetTarget = new_distance * camera_offset_unit;
}

void LLAgent::cameraOrbitIn(F32 meters)
{
//MK
	// If we have to force the camera distance because of RLV restrictions,
	// don't do anything else
	if (gRLenabled && !gRLInterface.checkCameraLimits(true))
	{
		return;
	}
//mk

	if (mFocusOnAvatar && mCameraMode == CAMERA_MODE_THIRD_PERSON)
	{
		static LLCachedControl<F32> camera_offset_scale(gSavedSettings,
														"CameraOffsetScale");
		F32 camera_offset_dist = llmax(0.001f,
									   mCameraOffsetDefault.length() *
									   (F32)camera_offset_scale);

		mCameraZoomFraction = (mTargetCameraDistance - meters) /
							  camera_offset_dist;

		if (!LLPipeline::sFreezeTime &&
			mCameraZoomFraction < MIN_ZOOM_FRACTION && meters > 0.f)
		{
			// No need to animate, camera is already there.
			changeCameraToMouselook(false);
		}

		mCameraZoomFraction = llclamp(mCameraZoomFraction,
									  MIN_ZOOM_FRACTION, MAX_ZOOM_FRACTION);
	}
	else
	{
		LLVector3d camera_offset_unit(mCameraFocusOffsetTarget);
		F32 current_distance = (F32)camera_offset_unit.normalize();
		F32 new_distance = current_distance - meters;
		F32 min_zoom = LAND_MIN_ZOOM;

		// Do not move through focus point
		if (mFocusObject.notNull())
		{
			if (mFocusObject->isAvatar())
			{
				min_zoom = AVATAR_MIN_ZOOM;
			}
			else
			{
				min_zoom = OBJECT_MIN_ZOOM;
			}
		}

		new_distance = llmax(new_distance, min_zoom);

		// Unless camera is unconstrained
		if (!noCameraConstraints())
		{
			// Do not zoom too far back
			constexpr F32 DIST_FUDGE = 16.f; // meters
			F32 region_with = mRegionp ? mRegionp->getWidth()
									   : REGION_WIDTH_METERS;
			F32 max_distance = llmin(mDrawDistance - DIST_FUDGE,
									 region_with - DIST_FUDGE);
			if (new_distance > max_distance)
			{
				new_distance = max_distance;
			}

			// Appearance editing mode constraints
			if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
			{
				new_distance = llclamp(new_distance, APPEARANCE_MIN_ZOOM,
									   APPEARANCE_MAX_ZOOM);
			}
		}

		// Compute new camera offset
		mCameraFocusOffsetTarget = new_distance * camera_offset_unit;
		cameraZoomIn(1.f);
	}
}

void LLAgent::cameraPanIn(F32 meters)
{
	LLVector3d at_axis;
	at_axis.set(gViewerCamera.getAtAxis());

	mFocusTargetGlobal += meters * at_axis;
	mFocusGlobal = mFocusTargetGlobal;
	// Do not enforce zoom constraints as this is the only way for users to get
	// past them easily
	updateFocusOffset();
	// NOTE: panning movements expect the camera to move exactly with the focus
	// target, not animated behind -Nyx
	mCameraSmoothingLastPositionGlobal = calcCameraPositionTargetGlobal();
}

void LLAgent::cameraPanLeft(F32 meters)
{
	LLVector3d left_axis;
	left_axis.set(gViewerCamera.getLeftAxis());

	mFocusTargetGlobal += meters * left_axis;
	mFocusGlobal = mFocusTargetGlobal;

	// Disable smoothing for camera pan
	mCameraSmoothingStop = true;

	cameraZoomIn(1.f);
	updateFocusOffset();
	// NOTE: panning movements expect the camera to move exactly with the focus
	// target, not animated behind - Nyx
	mCameraSmoothingLastPositionGlobal = calcCameraPositionTargetGlobal();
}

void LLAgent::cameraPanUp(F32 meters)
{
	LLVector3d up_axis;
	up_axis.set(gViewerCamera.getUpAxis());

	mFocusTargetGlobal += meters * up_axis;
	mFocusGlobal = mFocusTargetGlobal;

	// Disable smoothing for camera pan
	mCameraSmoothingStop = true;

	cameraZoomIn(1.f);
	updateFocusOffset();
	// NOTE: panning movements expect the camera to move exactly with the focus
	// target, not animated behind -Nyx
	mCameraSmoothingLastPositionGlobal = calcCameraPositionTargetGlobal();
}

void LLAgent::setKey(S32 direction, S32& key)
{
	if (direction > 0)
	{
		key = 1;
	}
	else if (direction < 0)
	{
		key = -1;
	}
	else
	{
		key = 0;
	}
}

void LLAgent::setControlFlags(U32 mask)
{
	U32 old_flags = mControlFlags;
	mControlFlags |= mask;
	mFlagsDirty = mControlFlags != old_flags;
	if (mask & AGENT_CONTROL_SIT_ON_GROUND)
	{
		mSittingOnGround = true;
	}
	else if (mask & AGENT_CONTROL_STAND_UP)
	{
		mSittingOnGround = false;
	}
}

void LLAgent::clearControlFlags(U32 mask)
{
	U32 old_flags = mControlFlags;
	mControlFlags &= ~mask;
	if (old_flags != mControlFlags)
	{
		mFlagsDirty = true;
	}
}

void LLAgent::resetControlFlags()
{
	if (mFlagsNeedReset)
	{
		mFlagsNeedReset = false;
		mFlagsDirty = false;
		// Reset all of the ephemeral flags; some flags are managed elsewhere
		mControlFlags &= AGENT_CONTROL_AWAY | AGENT_CONTROL_FLY |
						 AGENT_CONTROL_MOUSELOOK;
	}
}

void LLAgent::setAFK()
{
	if (!mRegionp)
	{
		// Do not set AFK if we are not talking to a region yet.
		return;
	}

	if (!(mControlFlags & AGENT_CONTROL_AWAY))
	{
		sendAnimationRequest(ANIM_AGENT_AWAY, ANIM_REQUEST_START);
		setControlFlags(AGENT_CONTROL_AWAY | AGENT_CONTROL_STOP);
		gAwayTimer.start();
		if (gAutomationp)
		{
			gAutomationp->onAgentOccupationChange(1);
		}
	}
}

void LLAgent::clearAFK()
{
	gAwayTriggerTimer.reset();

	// Gods can sometimes get into away state (via gestures) without setting
	// the appropriate control flag. JC
	if (mControlFlags & AGENT_CONTROL_AWAY ||
		(isAgentAvatarValid() &&
		 gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AWAY) !=
			gAgentAvatarp->mSignaledAnimations.end()))
	{
		sendAnimationRequest(ANIM_AGENT_AWAY, ANIM_REQUEST_STOP);
		clearControlFlags(AGENT_CONTROL_AWAY);
		if (gAutomationp && !mIsBusy && !mIsAutoReplying)
		{
			gAutomationp->onAgentOccupationChange(0);
		}
	}
}

void LLAgent::setBusy()
{
	mIsBusy = true;
	sendAnimationRequest(ANIM_AGENT_BUSY, ANIM_REQUEST_START);
	clearAutoReply();
	if (gAutomationp)
	{
		gAutomationp->onAgentOccupationChange(2);
	}
}

void LLAgent::clearBusy()
{
	mIsBusy = false;
	sendAnimationRequest(ANIM_AGENT_BUSY, ANIM_REQUEST_STOP);
	if (gAutomationp && !mIsAutoReplying && !getAFK())
	{
		gAutomationp->onAgentOccupationChange(0);
	}
}

void LLAgent::setAutoReply()
{
	mIsAutoReplying = true;
	clearBusy();
	if (gAutomationp)
	{
		gAutomationp->onAgentOccupationChange(3);
	}
}

void LLAgent::clearAutoReply()
{
	mIsAutoReplying = false;
	if (gAutomationp && !mIsBusy && !getAFK())
	{
		gAutomationp->onAgentOccupationChange(0);
	}
}

void LLAgent::propagate(F32 dt)
{
	// Update UI based on agent motion
	LLFloaterMove* floaterp = LLFloaterMove::getInstance();
	if (floaterp)
	{
		floaterp->mForwardButton->setToggleState(mAtKey > 0 || mWalkKey > 0);
		floaterp->mBackwardButton->setToggleState(mAtKey < 0 || mWalkKey < 0);
		floaterp->mSlideLeftButton->setToggleState(mLeftKey > 0);
		floaterp->mSlideRightButton->setToggleState(mLeftKey < 0);
		floaterp->mTurnLeftButton->setToggleState(mYawKey > 0.f);
		floaterp->mTurnRightButton->setToggleState(mYawKey < 0.f);
		floaterp->mMoveUpButton->setToggleState(mUpKey > 0);
		floaterp->mMoveDownButton->setToggleState(mUpKey < 0);
	}

	// Handle rotation based on keyboard levels
	constexpr F32 YAW_RATE = 90.f * DEG_TO_RAD;		// Radians per second
	yaw(YAW_RATE * mYawKey * dt);

	constexpr F32 PITCH_RATE = 90.f * DEG_TO_RAD;	// Radians per second
	pitch(PITCH_RATE * (F32) mPitchKey * dt);

	// Handle auto-land behavior
	static LLCachedControl<bool> automatic_fly(gSavedSettings, "AutomaticFly");
	if (automatic_fly && mUpKey < 0 && isAgentAvatarValid() &&
		!gAgentAvatarp->mInAir)
	{
		LLVector3 land_vel = getVelocity();
		land_vel.mV[VZ] = 0.f;
		if (land_vel.lengthSquared() < MAX_VELOCITY_AUTO_LAND_SQUARED)
		{
			// Land automatically
			setFlying(false);
		}
	}

	// Clear keys
	mAtKey = mWalkKey = mLeftKey = mUpKey = mPitchKey = 0;
	mYawKey = 0.f;
}

void LLAgent::checkPositionChanged()
{
	LLVector3d global_pos = getPositionGlobal();
	if (!mLastPositionGlobal.isExactlyZero())
	{
		LLVector3d delta = global_pos - mLastPositionGlobal;
		// Update the travel distance stat.
		mDistanceTraveled += delta.length();

		// Send the "position changed signal" if the position changed
		// by more than 3 meters, and throttle the signals by limiting
		// them to one every 10 seconds.
		if ((mLastPosGlobalTest - global_pos).lengthSquared() > 9.0 &&
			gFrameTimeSeconds - mLastPosGlobalSignaled > 10.f)
		{
			mLastPosGlobalSignaled = gFrameTimeSeconds;
			mLastPosGlobalTest = mPositionGlobal;
			// Send the signal to registered callbacks.
			mPosChangeSignal(mFrameAgent.getOrigin(), global_pos);
			// Not registered as a signal, because we want to update the
			// history regardless of an automation script usage (e.g. to
			// get it from LSL scripts). We call it on purpose *after* we
			// called the signals, which may include the signal callback used
			// to trigger in its turn the OnPositionChange() Lua callback (see
			// this callback documentation in the viewer Lua manual). HB
			HBViewerAutomation::addToAgentPosHistory(global_pos);
		}
	}
	mLastPositionGlobal = global_pos;
}

void LLAgent::updateAgentPosition(F32 dt, F32 yaw_radians, S32 mouse_x,
								  S32 mouse_y)
{
	propagate(dt);
	rotate(yaw_radians, 0.f, 0.f, 1.f);

	// Check for water and land collision, set underwater flag
	updateLookAt(mouse_x, mouse_y);

	// When agent has no parent, position updates come from setPositionAgent()
	// but when agent is seated (i.e. it is parented to the seat object), the
	// position remains unchanged relative to parent and no parent's position
	// update trigger setPositionAgent(); in this case we therefore need to
	// check for a change in position here.
	if (isAgentAvatarValid() && gAgentAvatarp->getParent())
	{
		checkPositionChanged();
	}
}

void LLAgent::updateLookAt(S32 mouse_x, S32 mouse_y)
{
	static LLVector3 last_at_axis;

	if (!isAgentAvatarValid())	// Also true when gViewerWindowp is NULL
	{
		return;
	}

	LLVector3 root_at = LLVector3::x_axis *
						gAgentAvatarp->mRoot->getWorldRotation();
	if (LLViewerWindow::getMouseVelocityStat().getCurrent() < 0.01f &&
		root_at * last_at_axis > 0.95f)
	{
		LLQuaternion av_inv_rot = ~gAgentAvatarp->mRoot->getWorldRotation();
		LLVector3 vel = gAgentAvatarp->getVelocity();
		if (vel.lengthSquared() > 4.f)
		{
			setLookAt(LOOKAT_TARGET_IDLE, gAgentAvatarp, vel * av_inv_rot);
		}
		else
		{
			// *FIX: rotate mFrameAgent by sit object's rotation ?

			// Use the camera current rotation
			LLQuaternion look_rotation =
				gAgentAvatarp->mIsSitting ? gAgentAvatarp->getRenderRotation()
										  : mFrameAgent.getQuaternion();
			LLVector3 look_offset = LLVector3(2.f, 0.f, 0.f) *
									look_rotation * av_inv_rot;
			setLookAt(LOOKAT_TARGET_IDLE, gAgentAvatarp, look_offset);
		}
		last_at_axis = root_at;
		return;
	}

	last_at_axis = root_at;

	if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		setLookAt(LOOKAT_TARGET_NONE, gAgentAvatarp,
				  LLVector3(-2.f, 0.f, 0.f));
	}
	else
	{
		// Move head based on cursor position
		ELookAtType lookat_type = LOOKAT_TARGET_NONE;
		LLCoordFrame cam_frame = (LLCoordFrame)gViewerCamera;

		if (cameraMouselook())
		{
			lookat_type = LOOKAT_TARGET_MOUSELOOK;
		}
		else if (cameraThirdPerson())
		{
			// Range from -.5 to .5
			F32 x_from_center = (F32)mouse_x /
								(F32)gViewerWindowp->getWindowWidth() - 0.5f;
			F32 y_from_center = (F32)mouse_y /
								(F32)gViewerWindowp->getWindowHeight() - 0.5f;

			static LLCachedControl<bool> eyes_follow_mouse(gSavedSettings,
														   "EyesFollowMousePointer");
			static LLCachedControl<F32> yaw_from_mouse_position(gSavedSettings,
																"YawFromMousePosition");
			static LLCachedControl<F32> pitch_from_mouse_position(gSavedSettings,
																  "PitchFromMousePosition");
			if (eyes_follow_mouse)
			{
				cam_frame.yaw(-x_from_center * yaw_from_mouse_position *
							  DEG_TO_RAD);
				cam_frame.pitch(-y_from_center * pitch_from_mouse_position *
								DEG_TO_RAD);
			}

			lookat_type = LOOKAT_TARGET_FREELOOK;
		}

		LLVector3 head_look_axis = cam_frame.getAtAxis();
#if 0	// RN: we use world-space offset for mouselook and freelook
		LLQuaternion av_inv_rot = ~gAgentAvatarp->mRoot->getWorldRotation();
		head_look_axis = head_look_axis * av_inv_rot;
#endif
		setLookAt(lookat_type, gAgentAvatarp, head_look_axis);
	}
}

std::ostream& operator<<(std::ostream& s, const LLAgent& agent)
{
	// This is unfinished, but might never be used. We will just leave it for
	// now; we can always delete it.
	s << " { " << "  Frame = " << agent.mFrameAgent << "\n" << " }";
	return s;
}

void LLAgent::setAvatarObject(LLVOAvatarSelf* avatar)
{
	if (!avatar)
	{
		llinfos << "NULL agent pointer passed: ignoring." << llendl;
		return;
	}

	if (!mLookAt)
	{
		mLookAt =
			(LLHUDEffectLookAt*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_LOOKAT);
	}
	if (!mPointAt)
	{
		mPointAt =
			(LLHUDEffectPointAt*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_POINTAT);
	}

	if (mLookAt.notNull())
	{
		mLookAt->setSourceObject(avatar);
	}
	if (mPointAt.notNull())
	{
		mPointAt->setSourceObject(avatar);
	}
}

// Returns true if your own avatar needs to be rendered. Usually only in third
// person and build.
bool LLAgent::needsRenderAvatar()
{
	if (cameraMouselook() && !LLVOAvatar::sVisibleInFirstPerson)
	{
		return false;
	}

	return mShowAvatar && mGenderChosen;
}

bool LLAgent::needsRenderHead()
{
	return (mShowAvatar && !cameraMouselook()) ||
		   (LLVOAvatar::sVisibleInFirstPerson &&
			LLPipeline::sReflectionRender);
}

void LLAgent::startTyping()
{
	mTypingTimer.reset();

	if (getRenderState() & AGENT_STATE_TYPING)
	{
		// Already typing, so do not trigger a different animation
		return;
	}
	setRenderState(AGENT_STATE_TYPING);

	if (mChatTimer.getElapsedTimeF32() < 2.f)
	{
		LLVOAvatar* chatter = gObjectList.findAvatar(mLastChatterID);
		if (chatter)
		{
			setLookAt(LOOKAT_TARGET_RESPOND, chatter, LLVector3::zero);
		}
	}

	if (gSavedSettings.getBool("PlayTypingAnim"))
	{
		sendAnimationRequest(ANIM_AGENT_TYPE, ANIM_REQUEST_START);
	}
	if (gChatBarp)
	{
		gChatBarp->sendChatFromViewer("", CHAT_TYPE_START, false);
	}
}

void LLAgent::stopTyping()
{
	if (mRenderState & AGENT_STATE_TYPING)
	{
		clearRenderState(AGENT_STATE_TYPING);
		sendAnimationRequest(ANIM_AGENT_TYPE, ANIM_REQUEST_STOP);
		if (gChatBarp)
		{
			gChatBarp->sendChatFromViewer("", CHAT_TYPE_STOP, false);
		}
	}
}

void LLAgent::setRenderState(U8 newstate)
{
	mRenderState |= newstate;
}

void LLAgent::clearRenderState(U8 clearstate)
{
	mRenderState &= ~clearstate;
}

U8 LLAgent::getRenderState()
{
	if (!gKeyboardp)
	{
		return 0;
	}

	// *FIX: do not do stuff in a getter !  This is infinite loop city !
	if (mTypingTimer.getElapsedTimeF32() > TYPING_TIMEOUT_SECS &&
		(mRenderState & AGENT_STATE_TYPING))
	{
		stopTyping();
	}

	if ((!gSelectMgr.getSelection()->isEmpty() &&
		 gSelectMgr.shouldShowSelection()) ||
		gToolMgr.getCurrentTool()->isEditing())
	{
		setRenderState(AGENT_STATE_EDITING);
	}
	else
	{
		clearRenderState(AGENT_STATE_EDITING);
	}

	return mRenderState;
}

static const LLFloaterView::skip_list_t& get_skip_list()
{
	static LLFloaterView::skip_list_t skip_list;
	skip_list.insert(LLFloaterMiniMap::getInstance());
	return skip_list;
}

void LLAgent::endAnimationUpdateUI()
{
	if (mCameraMode == mLastCameraMode)
	{
		// We are already done endAnimationUpdateUI for this transition.
		return;
	}

	// Clean up UI from mode we are leaving
	if (mLastCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		// Show mouse cursor
		if (gViewerWindowp)
		{
			gViewerWindowp->showCursor();
		}
		// Show menus
		if (gMenuBarViewp)
		{
			gMenuBarViewp->setVisible(true);
		}
		if (gStatusBarp)
		{
			gStatusBarp->setVisibleForMouselook(true);
		}

		gToolMgr.setCurrentToolset(gBasicToolset);

		// Only pop if we have pushed...
		if (mViewsPushed)
		{
			mViewsPushed = false;
			if (gFloaterViewp)
			{
				gFloaterViewp->popVisibleAll(get_skip_list());
			}
		}

		setLookAt(LOOKAT_TARGET_CLEAR);
		if (gMorphViewp)
		{
			gMorphViewp->setVisible(false);
		}

		// Disable mouselook-specific animations
		if (isAgentAvatarValid() &&
			gAgentAvatarp->isAnyAnimationSignaled(AGENT_GUN_AIM_ANIMS,
												  NUM_AGENT_GUN_AIM_ANIMS))
		{
			const LLVOAvatar::anim_map_t& anims =
				gAgentAvatarp->mSignaledAnimations;
			LLVOAvatar::anim_it_t end =
					gAgentAvatarp->mSignaledAnimations.end();
			if (anims.find(ANIM_AGENT_AIM_RIFLE_R) != end)
			{
				sendAnimationRequest(ANIM_AGENT_AIM_RIFLE_R,
									 ANIM_REQUEST_STOP);
				sendAnimationRequest(ANIM_AGENT_HOLD_RIFLE_R,
									 ANIM_REQUEST_START);
			}
			if (anims.find(ANIM_AGENT_AIM_HANDGUN_R) != end)
			{
				sendAnimationRequest(ANIM_AGENT_AIM_HANDGUN_R,
									 ANIM_REQUEST_STOP);
				sendAnimationRequest(ANIM_AGENT_HOLD_HANDGUN_R,
									 ANIM_REQUEST_START);
			}
			if (anims.find(ANIM_AGENT_AIM_BAZOOKA_R) != end)
			{
				sendAnimationRequest(ANIM_AGENT_AIM_BAZOOKA_R,
									 ANIM_REQUEST_STOP);
				sendAnimationRequest(ANIM_AGENT_HOLD_BAZOOKA_R,
									 ANIM_REQUEST_START);
			}
			if (anims.find(ANIM_AGENT_AIM_BOW_L) != end)
			{
				sendAnimationRequest(ANIM_AGENT_AIM_BOW_L,
									 ANIM_REQUEST_STOP);
				sendAnimationRequest(ANIM_AGENT_HOLD_BOW_L,
									 ANIM_REQUEST_START);
			}
		}
	}
	else if (mLastCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		// Make sure we ask to save changes
		gToolMgr.setCurrentToolset(gBasicToolset);

		// *HACK: If we are quitting and we were in customize avatar, do not
		// let the mini-map go visible again. JC
		if (!gAppViewerp->quitRequested())
		{
			LLFloaterMiniMap::getInstance()->popVisible();
		}

		if (gMorphViewp)
		{
			gMorphViewp->setVisible(false);
		}

		if (isAgentAvatarValid() && mCustomAnim)
		{
			sendAnimationRequest(ANIM_AGENT_CUSTOMIZE, ANIM_REQUEST_STOP);
			sendAnimationRequest(ANIM_AGENT_CUSTOMIZE_DONE,
								 ANIM_REQUEST_START);
			mCustomAnim = false;
		}

		setLookAt(LOOKAT_TARGET_CLEAR);
	}

	//---------------------------------------------------------------------
	// Set up UI for mode we're entering
	//---------------------------------------------------------------------
	if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		// Hide menus
		if (gMenuBarViewp)
		{
			gMenuBarViewp->setVisible(false);
		}
		if (gStatusBarp)
		{
			gStatusBarp->setVisibleForMouselook(false);
		}

		// Clear out camera lag effect
		mCameraLag.clear();

		// JC - Added for always chat in third person option
		gFocusMgr.setKeyboardFocus(NULL);

		gToolMgr.setCurrentToolset(gMouselookToolset);

		mViewsPushed = true;

		if (gFloaterViewp)
		{
			gFloaterViewp->pushVisibleAll(false, get_skip_list());
		}

		if (gMorphViewp)
		{
			gMorphViewp->setVisible(false);
		}

		if (gConsolep)
		{
			gConsolep->setVisible(true);
		}

		if (isAgentAvatarValid())
		{
			// Trigger mouselook-specific animations
			if (gAgentAvatarp->isAnyAnimationSignaled(AGENT_GUN_HOLD_ANIMS,
													  NUM_AGENT_GUN_HOLD_ANIMS))
			{
				const LLVOAvatar::anim_map_t& anims =
					gAgentAvatarp->mSignaledAnimations;
				LLVOAvatar::anim_it_t end =
					gAgentAvatarp->mSignaledAnimations.end();
				if (anims.find(ANIM_AGENT_HOLD_RIFLE_R) != end)
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_RIFLE_R,
										 ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_RIFLE_R,
										 ANIM_REQUEST_START);
				}
				if (anims.find(ANIM_AGENT_HOLD_HANDGUN_R) != end)
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_HANDGUN_R,
										 ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_HANDGUN_R,
										 ANIM_REQUEST_START);
				}
				if (anims.find(ANIM_AGENT_HOLD_BAZOOKA_R) != end)
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_BAZOOKA_R,
										 ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_BAZOOKA_R,
										 ANIM_REQUEST_START);
				}
				if (anims.find(ANIM_AGENT_HOLD_BOW_L) != end)
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_BOW_L,
										 ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_BOW_L,
										 ANIM_REQUEST_START);
				}
			}
			LLViewerObject* parentp =
				(LLViewerObject*)gAgentAvatarp->getParent();
			if (parentp)
			{
				LLVector3 at_axis = gViewerCamera.getAtAxis();
				LLViewerObject* root_object =
					(LLViewerObject*)gAgentAvatarp->getRoot();
				if (root_object->flagCameraDecoupled())
				{
					resetAxes(at_axis);
				}
				else
				{
					resetAxes(at_axis * ~(parentp->getRenderRotation()));
				}
			}
		}
	}
	else if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		gToolMgr.setCurrentToolset(gFaceEditToolset);

		LLFloaterMiniMap::getInstance()->pushVisible(false);

		if (gMorphViewp)
		{
			gMorphViewp->setVisible(true);
		}

		// Freeze avatar
		if (isAgentAvatarValid())
		{
			mPauseRequest = gAgentAvatarp->requestPause();
		}
	}

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->updateAttachmentVisibility(mCameraMode);
	}

	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}

	// Do not let this be called more than once if the camera mode has not
	// changed. - JC
	mLastCameraMode = mCameraMode;
}

void LLAgent::updateCamera()
{
	if (gCubeSnapshot)
	{
		return;
	}

	mCameraUpVector = LLVector3::z_axis;

	U32 camera_mode = mCameraAnimating ? mLastCameraMode : mCameraMode;

	validateFocusObject();

	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting &&
		camera_mode == CAMERA_MODE_MOUSELOOK)
	{
		mCameraUpVector = mCameraUpVector * gAgentAvatarp->getRenderRotation();
	}

	if (cameraThirdPerson() && mFocusOnAvatar &&
		LLFollowCamMgr::getActiveFollowCamParams())
	{
		changeCameraToFollow();
	}

	// NOTE: this needs to be integrated into a general upVector system here
	// within llAgent.
	if (camera_mode == CAMERA_MODE_FOLLOW && mFocusOnAvatar)
	{
		mCameraUpVector = mFollowCam.getUpVector();
	}

	if (mSitCameraEnabled && mSitCameraReferenceObject->isDead())
	{
		setSitCamera(LLUUID::null);
	}

	// Update UI with our camera inputs
	LLFloaterCamera* floater_camera = LLFloaterCamera::getInstance();
	floater_camera->mRotate->setToggleState(mOrbitRightKey > 0.f,	// left
											mOrbitUpKey > 0.f,		// top
											mOrbitLeftKey > 0.f,	// right
											mOrbitDownKey > 0.f);	// bottom

	floater_camera->mZoom->setToggleState(mOrbitInKey > 0.f,		// top
										  mOrbitOutKey > 0.f);		// bottom

	floater_camera->mTrack->setToggleState(mPanLeftKey > 0.f,		// left
										   mPanUpKey > 0.f,			// top
										   mPanRightKey > 0.f,		// right
										   mPanDownKey > 0.f);		// bottom

	// Handle camera movement based on keyboard.
	constexpr F32 ORBIT_OVER_RATE = 90.f * DEG_TO_RAD;		// Rad/s
	constexpr F32 ORBIT_AROUND_RATE = 90.f * DEG_TO_RAD;	// Rad/s
	constexpr F32 PAN_RATE = 5.f;							// M/s

	LLViewerCamera* camera = &gViewerCamera;

	if (mOrbitUpKey || mOrbitDownKey)
	{
		F32 input_rate = mOrbitUpKey - mOrbitDownKey;
		cameraOrbitOver(input_rate * ORBIT_OVER_RATE / gFPSClamped);
	}

	if (mOrbitLeftKey || mOrbitRightKey)
	{
		F32 input_rate = mOrbitLeftKey - mOrbitRightKey;
		cameraOrbitAround(input_rate * ORBIT_AROUND_RATE / gFPSClamped);
	}

	if (mOrbitInKey || mOrbitOutKey)
	{
		F32 input_rate = mOrbitInKey - mOrbitOutKey;

		LLVector3d to_focus = getPosGlobalFromAgent(camera->getOrigin()) -
							  calcFocusPositionTargetGlobal();
		F32 distance_to_focus = (F32)to_focus.length();
		// Move at distance (in meters) meters per second
		cameraOrbitIn(input_rate * distance_to_focus / gFPSClamped);
	}

	if (mPanInKey || mPanOutKey)
	{
		F32 input_rate = mPanInKey - mPanOutKey;
		cameraPanIn(input_rate * PAN_RATE / gFPSClamped);
	}

	if (mPanRightKey || mPanLeftKey)
	{
		F32 input_rate = mPanRightKey - mPanLeftKey;
		cameraPanLeft(input_rate * -PAN_RATE / gFPSClamped);
	}

	if (mPanUpKey || mPanDownKey)
	{
		F32 input_rate = mPanUpKey - mPanDownKey;
		cameraPanUp(input_rate * PAN_RATE / gFPSClamped);
	}

	// Clear camera keyboard keys.
	mOrbitLeftKey = mOrbitRightKey = mOrbitUpKey = mOrbitDownKey =
					mOrbitInKey = mOrbitOutKey = 0.f;

	mPanRightKey = mPanLeftKey = mPanUpKey = mPanDownKey = mPanInKey =
				   mPanOutKey = 0.f;

	// lerp camera focus offset
	mCameraFocusOffset =
		lerp(mCameraFocusOffset, mCameraFocusOffsetTarget,
			 LLCriticalDamp::getInterpolant(CAMERA_FOCUS_HALF_LIFE));

	if (mCameraMode == CAMERA_MODE_FOLLOW && isAgentAvatarValid())
	{
		// This is where the avatar's position and rotation are given to
		// followCam, and where it is updated. All three of its attributes are
		// updated: (1) position, (2) focus, and (3) upvector. They can then be
		// queried elsewhere in llAgent.

		LLFollowCamParams* curr_cam = LLFollowCamMgr::getActiveFollowCamParams();
		if (curr_cam)
		{
			mFollowCam.copyParams(*curr_cam);
			// *TODO: use combined rotation of frame agent and sit object
			LLQuaternion av_rot =
				gAgentAvatarp->mIsSitting ? gAgentAvatarp->getRenderRotation()
										  : mFrameAgent.getQuaternion();
			mFollowCam.setSubjectPositionAndRotation(gAgentAvatarp->getRenderPosition(),
													 av_rot);
			mFollowCam.update();
		}
		else
		{
			changeCameraToThirdPerson(true);
		}
	}

	LLVector3d camera_pos_global;
	LLVector3d camera_target_global = calcCameraPositionTargetGlobal();
	mCameraVirtualPositionAgent = getPosAgentFromGlobal(camera_target_global);
	LLVector3d focus_target_global = calcFocusPositionTargetGlobal();

	// Perform field of view correction
	mCameraFOVZoomFactor = calcCameraFOVZoomFactor();
//MK
	if (!gRLenabled || gRLInterface.mCamDistMax >= EXTREMUM * 0.75)
//mk
	{
		camera_target_global = focus_target_global +
							   (camera_target_global - focus_target_global) *
							   (1.f + mCameraFOVZoomFactor);
	}

	mShowAvatar = true; // can see avatar by default

	// Adjust position for animation
	if (mCameraAnimating)
	{
		F32 time = mAnimationTimer.getElapsedTimeF32();

#if 0
		// Yet another instance of critically damped motion, hooray !
		F32 fraction_of_animation = 1.f - powf(2.f,
											   -time / CAMERA_ZOOM_HALF_LIFE);
#else
		// Linear interpolation
		F32 fraction_of_animation = time / mAnimationDuration;
#endif

		F32 fraction_animation_to_skip = 0.f;
		if (mAnimationCameraStartGlobal != camera_target_global)
		{
			LLVector3d cam_delta = mAnimationCameraStartGlobal -
								   camera_target_global;
			fraction_animation_to_skip = HEAD_BUFFER_SIZE /
										 (F32)cam_delta.length();
		}

		F32 animation_start_fraction =
			mLastCameraMode == CAMERA_MODE_MOUSELOOK ?
				fraction_animation_to_skip : 0.f;
		F32 animation_finish_fraction =
			mCameraMode == CAMERA_MODE_MOUSELOOK ?
				1.f - fraction_animation_to_skip : 1.f;

		if (fraction_of_animation < animation_finish_fraction)
		{
			if (fraction_of_animation < animation_start_fraction ||
				fraction_of_animation > animation_finish_fraction)
			{
				mShowAvatar = false;
			}

			// Adjust position for animation
			F32 smooth_fraction_of_animation =
				llsmoothstep(0.f, 1.f, fraction_of_animation);
			camera_pos_global = lerp(mAnimationCameraStartGlobal,
									 camera_target_global,
									 smooth_fraction_of_animation);
			mFocusGlobal = lerp(mAnimationFocusStartGlobal,
								focus_target_global,
								smooth_fraction_of_animation);
		}
		else
		{
			// Animation complete
			mCameraAnimating = false;

			camera_pos_global = camera_target_global;
			mFocusGlobal = focus_target_global;

			endAnimationUpdateUI();
			mShowAvatar = true;
		}

		if (isAgentAvatarValid() && mCameraMode != CAMERA_MODE_MOUSELOOK)
		{
			gAgentAvatarp->updateAttachmentVisibility(mCameraMode);
		}
	}
	else
	{
		camera_pos_global = camera_target_global;
		mFocusGlobal = focus_target_global;
		mShowAvatar = true;
	}

	// Smoothing

	LLVector3d agent_pos = getPositionGlobal();
	LLVector3d camera_pos_agent = camera_pos_global - agent_pos;
	// Sitting on what you're manipulating can cause camera jitter with
	// smoothing. This turns off smoothing while editing. -MG
	mCameraSmoothingStop = mCameraSmoothingStop || gToolMgr.inBuildMode();
	if (cameraThirdPerson() && !mCameraSmoothingStop)
	{
		constexpr F32 SMOOTHING_HALF_LIFE = 0.02f;

		static LLCachedControl<F32> camera_position_smoothing(gSavedSettings,
															  "CameraPositionSmoothing");
		F32 smoothing =
			LLCriticalDamp::getInterpolant(camera_position_smoothing *
										   SMOOTHING_HALF_LIFE,
										   false);

		if (mFocusOnAvatar && !mFocusObject) // We differentiate on avatar mode
		{
			// For avatar-relative focus, we smooth in avatar space; the
			// avatar moves too jerkily w/r/t global space to smooth there.

			LLVector3d delta = camera_pos_agent - mCameraSmoothingLastPositionAgent;
			// Only smooth over short distances please
			if (delta.length() < MAX_CAMERA_SMOOTH_DISTANCE)
			{
				camera_pos_agent = lerp(mCameraSmoothingLastPositionAgent,
										camera_pos_agent, smoothing);
				camera_pos_global = camera_pos_agent + agent_pos;
			}
		}
		else
		{
			LLVector3d delta = camera_pos_global -
							   mCameraSmoothingLastPositionGlobal;
			// Only smooth over short distances please
			if (delta.length() < MAX_CAMERA_SMOOTH_DISTANCE)
			{
				camera_pos_global = lerp(mCameraSmoothingLastPositionGlobal,
										 camera_pos_global, smoothing);
			}
		}
	}
	mCameraSmoothingLastPositionGlobal = camera_pos_global;
	mCameraSmoothingLastPositionAgent = camera_pos_agent;
	mCameraSmoothingStop = false;

	mCameraCurrentFOVZoomFactor =
		lerp(mCameraCurrentFOVZoomFactor, mCameraFOVZoomFactor,
			 LLCriticalDamp::getInterpolant(FOV_ZOOM_HALF_LIFE));
#if 0	// Too spammy...
	LL_DEBUGS("Agent") << "Current FOV Zoom: " << mCameraCurrentFOVZoomFactor
					   << " - Target FOV Zoom: " << mCameraFOVZoomFactor
					   << " - Object penetration: " << mFocusObjectDist
					   << LL_ENDL;
#endif
	F32 ui_offset = 0.f;
	if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		ui_offset = calcCustomizeAvatarUIOffset(camera_pos_global);
	}

	LLVector3 focus_agent = getPosAgentFromGlobal(mFocusGlobal);

	mCameraPositionAgent = getPosAgentFromGlobal(camera_pos_global);

	// Move the camera

	camera->updateCameraLocation(mCameraPositionAgent, mCameraUpVector,
								 focus_agent);

	// RN: translate UI offset after camera is oriented properly
	camera->translate(camera->getLeftAxis() * ui_offset);

	// Change FOV
	camera->setView(camera->getDefaultFOV() /
					(1.f + mCameraCurrentFOVZoomFactor));

	// Follow camera when in customize mode
	if (cameraCustomizeAvatar())
	{
		setLookAt(LOOKAT_TARGET_FOCUS, NULL, mCameraPositionAgent);
	}

	checkPositionChanged();

	if (LLVOAvatar::sVisibleInFirstPerson && isAgentAvatarValid() &&
		!gAgentAvatarp->mIsSitting && cameraMouselook())
	{
		LLVector3 head_pos = gAgentAvatarp->mHeadp->getWorldPosition() +
							 LLVector3(0.08f, 0.f, 0.05f) *
							 gAgentAvatarp->mHeadp->getWorldRotation() +
							 LLVector3(0.1f, 0.f, 0.f) *
							 gAgentAvatarp->mPelvisp->getWorldRotation();
		LLVector3 diff = mCameraPositionAgent - head_pos;
		diff = diff * ~gAgentAvatarp->mRoot->getWorldRotation();

		LLJoint* torso_joint = gAgentAvatarp->mTorsop;
		LLJoint* chest_joint = gAgentAvatarp->mChestp;
		LLVector3 torso_scale = torso_joint->getScale();
		LLVector3 chest_scale = chest_joint->getScale();

		// Shorten avatar skeleton to avoid foot interpenetration
		if (!gAgentAvatarp->mInAir)
		{
			LLVector3 chest_offset =
				LLVector3(0.f, 0.f, chest_joint->getPosition().mV[VZ]) *
				torso_joint->getWorldRotation();
			F32 z_compensate = llclamp(-diff.mV[VZ], -0.2f, 1.f);
			F32 scale_factor = llclamp(1.f - (z_compensate * 0.5f /
											  chest_offset.mV[VZ]),
									   0.5f, 1.2f);
			torso_joint->setScale(LLVector3(1.f, 1.f, scale_factor));

			LLJoint* neck_joint = gAgentAvatarp->mNeckp;
			LLVector3 neck_offset =
				LLVector3(0.f, 0.f, neck_joint->getPosition().mV[VZ]) *
				chest_joint->getWorldRotation();
			scale_factor = llclamp(1.f - (z_compensate * 0.5f /
										  neck_offset.mV[VZ]),
								   0.5f, 1.2f);
			chest_joint->setScale(LLVector3(1.f, 1.f, scale_factor));
			diff.mV[VZ] = 0.f;
		}

		gAgentAvatarp->mPelvisp->setPosition(gAgentAvatarp->mPelvisp->getPosition() +
											 diff);

		gAgentAvatarp->mRoot->updateWorldMatrixChildren();

		for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
			 i < count; ++i)
		{
			LLViewerObject* object =
				gAgentAvatarp->mAttachedObjectsVector[i].first;
			if (object && !object->isDead() && object->mDrawable.notNull())
			{
				// Clear any existing "early" movements of attachment
				object->mDrawable->clearState(LLDrawable::EARLY_MOVE);
				gPipeline.updateMoveNormalAsync(object->mDrawable);
				object->updateText();
			}
		}

		torso_joint->setScale(torso_scale);
		chest_joint->setScale(chest_scale);
	}

//MK
	if (gRLenabled && mCameraMode != CAMERA_MODE_FOLLOW)
	{
		bool is_first_person = mCameraMode == CAMERA_MODE_MOUSELOOK;
		if (is_first_person && gRLInterface.mCamDistMin > 0.f)
		{
			changeCameraToDefault();
		}
		else if (!is_first_person && gRLInterface.mCamDistMax <= 0.f)
		{
			changeCameraToMouselook();
		}
	}
//mk
}

void LLAgent::updateFocusOffset()
{
	validateFocusObject();
	if (mFocusObject.notNull())
	{
		LLVector3d obj_pos =
			getPosGlobalFromAgent(mFocusObject->getRenderPosition());
		mFocusObjectOffset.set(mFocusTargetGlobal - obj_pos);
	}
}

void LLAgent::validateFocusObject()
{
	if (mFocusObject.notNull() && mFocusObject->isDead())
	{
		mFocusObjectOffset.clear();
		clearFocusObject();
		mCameraFOVZoomFactor = 0.f;
	}
}

F32 LLAgent::calcCustomizeAvatarUIOffset(const LLVector3d& camera_pos_global)
{
	F32 ui_offset = 0.f;

	if (gFloaterCustomizep && gViewerWindowp)
	{
		const LLRect& rect = gFloaterCustomizep->getRect();

		// Move the camera so that the avatar is not covered up by this floater
		F32 ratio = (F32)rect.getWidth() /
					(F32)gViewerWindowp->getWindowWidth();
		F32 fraction_of_fov = 0.5f - (0.5f * (1.f - llmin(1.f, ratio)));
		F32 apparent_angle = fraction_of_fov * gViewerCamera.getView() *
							 gViewerCamera.getAspect(); // in radians
		F32 offset = tanf(apparent_angle);

		if (rect.mLeft < gViewerWindowp->getWindowWidth() - rect.mRight)
		{
			// Move the avatar to the right (camera to the left)
			ui_offset = offset;
		}
		else
		{
			// Move the avatar to the left (camera to the right)
			ui_offset = -offset;
		}
	}
	F32 range = (F32)dist_vec(camera_pos_global, getFocusGlobal());
	mUIOffset = lerp(mUIOffset, ui_offset,
					 LLCriticalDamp::getInterpolant(0.05f));
	return mUIOffset * range;
}

LLVector3d LLAgent::calcFocusPositionTargetGlobal()
{
	if (mFocusObject.notNull() && mFocusObject->isDead())
	{
		clearFocusObject();
	}

	if (mCameraMode == CAMERA_MODE_FOLLOW && mFocusOnAvatar)
	{
		mFocusTargetGlobal =
			getPosGlobalFromAgent(mFollowCam.getSimulatedFocus());
		return mFocusTargetGlobal;
	}

	if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		LLVector3d at_axis(1.0, 0.0, 0.0);
		LLQuaternion agent_rot = mFrameAgent.getQuaternion();
		if (isAgentAvatarValid())
		{
			LLViewerObject* parentp =
				(LLViewerObject*)gAgentAvatarp->getParent();
			if (parentp)
			{
				LLViewerObject* root_object =
					(LLViewerObject*)gAgentAvatarp->getRoot();
				if (!root_object->flagCameraDecoupled())
				{
					agent_rot *= parentp->getRenderRotation();
				}
			}
		}
		at_axis = at_axis * agent_rot;
		mFocusTargetGlobal = calcCameraPositionTargetGlobal() + at_axis;
		return mFocusTargetGlobal;
	}

	if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		return mFocusTargetGlobal;
	}

	if (!mFocusOnAvatar)
	{
		if (mFocusObject.notNull() && !mFocusObject->isDead() &&
			mFocusObject->mDrawable.notNull())
		{
			LLDrawable* drawablep = mFocusObject->mDrawable;
			if (mTrackFocusObject && drawablep && drawablep->isActive())
			{
				if (!mFocusObject->isAvatar())
				{
					if (mFocusObject->isSelected())
					{
						gPipeline.updateMoveNormalAsync(drawablep);
					}
					else if (drawablep->isState(LLDrawable::MOVE_UNDAMPED))
					{
						gPipeline.updateMoveNormalAsync(drawablep);
					}
					else
					{
						gPipeline.updateMoveDampedAsync(drawablep);
					}
				}
			}
			else
			{
				// If not tracking object, update offset based on new object
				// position
				updateFocusOffset();
			}
			LLVector3 focus_agent = mFocusObject->getRenderPosition() +
									mFocusObjectOffset;
			mFocusTargetGlobal.set(getPosGlobalFromAgent(focus_agent));
		}
		return mFocusTargetGlobal;
	}

	if (mSitCameraEnabled && isAgentAvatarValid() &&
			 gAgentAvatarp->mIsSitting && mSitCameraReferenceObject.notNull())
	{
		// Sit camera
		LLVector3 object_pos = mSitCameraReferenceObject->getRenderPosition();
		LLQuaternion object_rot = mSitCameraReferenceObject->getRenderRotation();

		LLVector3 target_pos = object_pos + (mSitCameraFocus * object_rot);
		return getPosGlobalFromAgent(target_pos);
	}

	return getPositionGlobal() + calcThirdPersonFocusOffset();
}

LLVector3d LLAgent::calcThirdPersonFocusOffset()
{
	// Offset from avatar
	LLVector3d focus_offset;
	focus_offset.set(mCameraFocusOffsetDefault);

	LLQuaternion agent_rot = mFrameAgent.getQuaternion();
	if (isAgentAvatarValid())
	{
		LLViewerObject* parentp = (LLViewerObject*)gAgentAvatarp->getParent();
		if (parentp)
		{
			agent_rot *= parentp->getRenderRotation();
		}
	}

	return focus_offset * agent_rot;
}

void LLAgent::setupSitCamera()
{
	if (!isAgentAvatarValid()) return;

	// Agent frame entering this function is in world coordinates
	LLViewerObject* parentp = (LLViewerObject*)gAgentAvatarp->getParent();
	if (parentp)
	{
		LLQuaternion parent_rot = parentp->getRenderRotation();
		// Slam agent coordinate frame to proper parent local version
		LLVector3 at_axis = mFrameAgent.getAtAxis();
		at_axis.mV[VZ] = 0.f;
		at_axis.normalize();
		resetAxes(at_axis * ~parent_rot);
	}
}

void LLAgent::setupCameraView(bool reset)
{
	static bool rear_view = false;

	bool new_rear_view = gSavedSettings.getBool("CameraFrontView");
	if (new_rear_view &&
		(mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR ||
		 mCameraMode == CAMERA_MODE_MOUSELOOK || reset))
	{
		gSavedSettings.setBool("CameraFrontView", false);
		new_rear_view = false;
	}
	if (new_rear_view)
	{
		mCameraFocusOffsetDefault =
			gSavedSettings.getVector3("FocusOffsetFrontView");
		mCameraOffsetDefault =
			gSavedSettings.getVector3("CameraOffsetFrontView");
	}
	else
	{
		mCameraFocusOffsetDefault =
			gSavedSettings.getVector3("FocusOffsetDefault");
		mCameraOffsetDefault =
			gSavedSettings.getVector3("CameraOffsetDefault");
	}
	if (rear_view != new_rear_view)
	{
		rear_view = new_rear_view;
		updateCamera();
	}
}

const LLVector3& LLAgent::getCameraPositionAgent() const
{
	return gViewerCamera.getOrigin();
}

LLVector3d LLAgent::getCameraPositionGlobal() const
{
	return getPosGlobalFromAgent(gViewerCamera.getOrigin());
}

F32 LLAgent::getHUDTargetZoom() const
{
	static LLCachedControl<F32> scale(gSavedSettings, "HUDScaleFactor");
	F32 zoom = scale;
	LLObjectSelectionHandle sel = gSelectMgr.getSelection();
	if (sel->getObjectCount() && sel->getSelectType() == SELECT_TYPE_HUD)
	{
		zoom *= mHUDTargetZoom;
	}
	return zoom;
}

F32	LLAgent::calcCameraFOVZoomFactor()
{
	LLVector3 camera_offset_dir;
	camera_offset_dir.set(mCameraFocusOffset);

	if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		return 0.f;
	}

	 // If not focusing on avatar or land
	if (!mFocusOnAvatar && mFocusObject.notNull() && !mFocusObject->isAvatar())
	{
		// Do not FOV zoom on mostly transparent objects
		F32 obj_min_dist = 0.f;
		if (!noCameraConstraints())
		{
			calcCameraMinDistance(obj_min_dist);
		}
		F32 current_distance = llmax(0.001f, camera_offset_dir.length());

		mFocusObjectDist = obj_min_dist - current_distance;

		F32 new_fov_zoom = llclamp(mFocusObjectDist / current_distance,
								   0.f, 1000.f);
		return new_fov_zoom;
	}

	// Keep old field of view until user changes focus explicitly
	return mCameraFOVZoomFactor;
}

LLVector3d LLAgent::calcCameraPositionTargetGlobal(bool* hit_limit)
{
	// Compute base camera position and look-at points.
	F32 camera_land_height;
	LLVector3d frame_center_global;
	if (isAgentAvatarValid())
	{
		frame_center_global =
			getPosGlobalFromAgent(gAgentAvatarp->mRoot->getWorldPosition());
	}
	else
	{
		frame_center_global = getPositionGlobal();
	}

	bool constrained = false;
	LLVector3d head_offset;
	head_offset.set(mThirdPersonHeadOffset);

	LLVector3d camera_position_global;

	if (mCameraMode == CAMERA_MODE_FOLLOW && mFocusOnAvatar)
	{
		camera_position_global =
			getPosGlobalFromAgent(mFollowCam.getSimulatedPosition());
	}
	else if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		if (!isAgentAvatarValid() || gAgentAvatarp->mDrawable.isNull())
		{
			llwarns << "Null avatar drawable !" << llendl;
			return LLVector3d::zero;
		}

		head_offset.clear();

		F32 fixup = 0.f;
		if (gAgentAvatarp->mIsSitting)
		{
			head_offset.mdV[VZ] += 0.1;
		}
		else if (gAgentAvatarp->hasPelvisFixup(fixup))
		{
			head_offset.mdV[VZ] -= fixup;
		}
		
		LLViewerObject* parentp = (LLViewerObject*)gAgentAvatarp->getParent();
		if (gAgentAvatarp->mIsSitting && parentp)
		{
			gAgentAvatarp->updateHeadOffset();
			head_offset.mdV[VX] = gAgentAvatarp->mHeadOffset.mV[VX];
			head_offset.mdV[VY] = gAgentAvatarp->mHeadOffset.mV[VY];
			head_offset.mdV[VZ] += gAgentAvatarp->mHeadOffset.mV[VZ];
			const LLMatrix4& mat = parentp->getRenderMatrix();
			camera_position_global =
				getPosGlobalFromAgent((gAgentAvatarp->getPosition() +
									   LLVector3(head_offset) *
									   gAgentAvatarp->getRotation()) * mat);
		}
		else
		{
			head_offset.mdV[VZ] += gAgentAvatarp->mHeadOffset.mV[VZ];
			camera_position_global =
				getPosGlobalFromAgent(gAgentAvatarp->getRenderPosition());
			head_offset = head_offset * gAgentAvatarp->getRenderRotation();
			camera_position_global = camera_position_global + head_offset;
		}
	}
	else if (mCameraMode == CAMERA_MODE_THIRD_PERSON && mFocusOnAvatar)
	{
		LLVector3 local_camera_offset;
		F32 camera_distance = 0.f;

		if (mSitCameraEnabled && isAgentAvatarValid() &&
			gAgentAvatarp->mIsSitting && mSitCameraReferenceObject.notNull())
		{
			// sit camera
			LLVector3 object_pos =
				mSitCameraReferenceObject->getRenderPosition();
			LLQuaternion object_rot =
				mSitCameraReferenceObject->getRenderRotation();

			LLVector3 target_pos = object_pos + mSitCameraPos * object_rot;

			camera_position_global = getPosGlobalFromAgent(target_pos);
		}
		else
		{
			static LLCachedControl<F32> camera_offset_scale(gSavedSettings,
															"CameraOffsetScale");
			local_camera_offset = mCameraZoomFraction *
								  mCameraOffsetDefault * camera_offset_scale;


			LLViewerObject* parentp = NULL;
			if (isAgentAvatarValid())
			{
				parentp = (LLViewerObject*)gAgentAvatarp->getParent();
			}
			// Are we sitting down ?
			if (parentp)
			{
				LLQuaternion parent_rot = parentp->getRenderRotation();
				// Slam agent coordinate frame to proper parent local version
				LLVector3 at_axis = mFrameAgent.getAtAxis() * parent_rot;
				at_axis.mV[VZ] = 0.f;
				at_axis.normalize();
				resetAxes(at_axis * ~parent_rot);

				local_camera_offset = local_camera_offset *
									  mFrameAgent.getQuaternion() * parent_rot;
			}
			else
			{
				local_camera_offset =
					mFrameAgent.rotateToAbsolute(local_camera_offset);
			}

			static LLCachedControl<bool> ignore_collisions(gSavedSettings,
														   "CameraIgnoreCollisions");
			if (!ignore_collisions && !mCameraCollidePlane.isExactlyZero() &&
				!(isAgentAvatarValid() && gAgentAvatarp->mIsSitting))
			{
				LLVector3 plane_normal;
				plane_normal.set(mCameraCollidePlane.mV);

				F32 offset_dot_norm = local_camera_offset * plane_normal;
				if (fabsf(offset_dot_norm) < 0.001f)
				{
					offset_dot_norm = 0.001f;
				}

				camera_distance = local_camera_offset.normalize();

				F32 pos_dot_norm = getPosAgentFromGlobal(frame_center_global +
														 head_offset) *
								   plane_normal;

				// If agent is outside the colliding half-plane
				if (pos_dot_norm > mCameraCollidePlane.mV[VW])
				{
					// Check to see if camera is on the opposite side (inside)
					// the half-plane
					if (offset_dot_norm + pos_dot_norm <
							mCameraCollidePlane.mV[VW])
					{
						// Diminish offset by factor to push it back outside
						// the half-plane
						camera_distance *= (pos_dot_norm -
											mCameraCollidePlane.mV[VW] -
											CAMERA_COLLIDE_EPSILON) /
										   -offset_dot_norm;
					}
				}
				else if (offset_dot_norm + pos_dot_norm >
							mCameraCollidePlane.mV[VW])
				{
					camera_distance *= (mCameraCollidePlane.mV[VW] -
										pos_dot_norm -
										CAMERA_COLLIDE_EPSILON) /
									   offset_dot_norm;
				}
			}
			else
			{
				camera_distance = local_camera_offset.normalize();
			}

			mTargetCameraDistance = llmax(camera_distance,
										  MIN_CAMERA_DISTANCE);

			if (mTargetCameraDistance != mCurrentCameraDistance)
			{
				F32 camera_lerp_amt =
					LLCriticalDamp::getInterpolant(CAMERA_ZOOM_HALF_LIFE);

				mCurrentCameraDistance = lerp(mCurrentCameraDistance,
											  mTargetCameraDistance,
											  camera_lerp_amt);
			}

			// Make the camera distance current
			local_camera_offset *= mCurrentCameraDistance;

			// Set the global camera position
			LLVector3d camera_offset;
			camera_offset.set(local_camera_offset);
			camera_position_global = frame_center_global + head_offset +
									 camera_offset;

			if (isAgentAvatarValid())
			{
				LLVector3d camera_lag_d;
				F32 lag_interp =
					LLCriticalDamp::getInterpolant(CAMERA_LAG_HALF_LIFE);
				LLVector3 target_lag;
				LLVector3 vel = getVelocity();

				// Lag by appropriate amount for flying
				F32 time_in_air =
					gAgentAvatarp->mTimeInAir.getElapsedTimeF32();
				if (!mCameraAnimating && gAgentAvatarp->mInAir &&
					time_in_air > GROUND_TO_AIR_CAMERA_TRANSITION_START_TIME)
				{
					LLVector3 frame_at_axis = mFrameAgent.getAtAxis();
					frame_at_axis -= projected_vec(frame_at_axis,
												   getReferenceUpVector());
					frame_at_axis.normalize();

					// Transition smoothly in air mode, to avoid camera pop
					F32 u = (time_in_air -
							 GROUND_TO_AIR_CAMERA_TRANSITION_START_TIME) /
							GROUND_TO_AIR_CAMERA_TRANSITION_TIME;
					u = llclamp(u, 0.f, 1.f);

					lag_interp *= u;

					if (gViewerWindowp->getLeftMouseDown() &&
						gViewerWindowp->getLastPick().mObjectID ==
							gAgentAvatarp->getID())
					{
						// Disable camera lag when using mouse-directed
						// steering
						target_lag.clear();
					}
					else
					{
						static LLCachedControl<F32> strength(gSavedSettings,
															 "DynamicCameraStrength");
						target_lag = vel * strength / 30.f;
					}

					mCameraLag = lerp(mCameraLag, target_lag, lag_interp);

					F32 lag_dist = mCameraLag.length();
					if (lag_dist > MAX_CAMERA_LAG)
					{
						mCameraLag = mCameraLag * MAX_CAMERA_LAG / lag_dist;
					}

					// Clamp camera lag so that avatar is always in front
					F32 dot = (mCameraLag - frame_at_axis *
							   MIN_CAMERA_LAG * u) * frame_at_axis;
					if (dot < -(MIN_CAMERA_LAG * u))
					{
						mCameraLag -= (dot + MIN_CAMERA_LAG * u) *
									  frame_at_axis;
					}
				}
				else
				{
					mCameraLag = lerp(mCameraLag, LLVector3::zero,
									  LLCriticalDamp::getInterpolant(0.15f));
				}

				camera_lag_d.set(mCameraLag);
				camera_position_global = camera_position_global - camera_lag_d;
			}
		}
	}
	else
	{
		LLVector3d focusPosGlobal = calcFocusPositionTargetGlobal();
		// Camera gets pushed out later WRT mCameraFOVZoomFactor... this is
		// "raw" value
		camera_position_global = focusPosGlobal + mCameraFocusOffset;
	}

	if (!noCameraConstraints() && !isGodlike())
	{
		LLViewerRegion* regionp =
			gWorld.getRegionFromPosGlobal(camera_position_global);
		bool constrain = true;
		if (regionp && regionp->canManageEstate())
		{
			constrain = false;
		}
		if (constrain)
		{
			F32 max_dist = mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR ?
							APPEARANCE_MAX_ZOOM : mDrawDistance;

			LLVector3d camera_offset = camera_position_global -
									   getPositionGlobal();
			F32 camera_distance = (F32)camera_offset.length();
			if (camera_distance > max_dist)
			{
				camera_position_global = getPositionGlobal() +
										 max_dist / camera_distance *
										 camera_offset;
				constrained = true;
			}
		}
	}

	// Do not let camera go underground
	F32 camera_min_off_ground = getCameraMinOffGround();

	camera_land_height = gWorld.resolveLandHeightGlobal(camera_position_global);

	if (camera_position_global.mdV[VZ] < camera_land_height +
										 camera_min_off_ground)
	{
		camera_position_global.mdV[VZ] = camera_land_height +
										 camera_min_off_ground;
		constrained = true;
	}

//MK
	// Constrain the distance by RLV restrictions here. Do not do it for
	// mouse-look because it would force the camera to the crotch.
	if (gRLenabled && mCameraMode != CAMERA_MODE_MOUSELOOK &&
		(gRLInterface.mCamDistMax < EXTREMUM ||
		 gRLInterface.mCamDistMin > -EXTREMUM))
	{
		LLJoint* ref_joint = gRLInterface.getCamDistDrawFromJoint();
		if (ref_joint)
		{
			LLVector3 joint_pos = ref_joint->getWorldPosition();
			LLVector3d joint_pos_3d = getPosGlobalFromAgent(joint_pos);
			LLVector3d camera_offset = camera_position_global - joint_pos_3d;
			F32 camera_distance = (F32)camera_offset.length();
			if (camera_distance != 0.f)
			{
				if (camera_distance > gRLInterface.mCamDistMax)
				{
					camera_position_global = joint_pos_3d +
											 (gRLInterface.mCamDistMax /
											  camera_distance) * camera_offset;
					constrained = true;
				}
				else if (camera_distance < gRLInterface.mCamDistMin)
				{
					camera_position_global = joint_pos_3d +
											 (gRLInterface.mCamDistMin /
											  camera_distance) * camera_offset;
					constrained = true;
				}
			}
		}
	}
//mk

	if (hit_limit)
	{
		*hit_limit = constrained;
	}

	return camera_position_global;
}

void LLAgent::handleScrollWheel(S32 clicks)
{
	if (mCameraMode == CAMERA_MODE_FOLLOW && getFocusOnAvatar())
	{
		// Not if the followCam position is locked in place
		if (!mFollowCam.getPositionLocked())
		{
			mFollowCam.zoom(clicks);
			if (mFollowCam.isZoomedToMinimumDistance())
			{
				changeCameraToMouselook(false);
			}
		}
	}
	else
	{
		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		// gcc accepts constexpr here, but not clang...
		static const F32 ROOT_ROOT_TWO = sqrtf(F_SQRT2);

		// Block if camera is animating
		if (mCameraAnimating)
		{
			return;
		}

		if (selection->getObjectCount() &&
			selection->getSelectType() == SELECT_TYPE_HUD)
		{
			F32 zoom_factor = powf(0.8f, -clicks);
			cameraZoomIn(zoom_factor);
		}
		else if (mFocusOnAvatar && mCameraMode == CAMERA_MODE_THIRD_PERSON)
		{
			static LLCachedControl<F32> camera_offset_scale(gSavedSettings,
															"CameraOffsetScale");
			F32 current_zoom_fraction = mTargetCameraDistance /
										(mCameraOffsetDefault.length() *
										 camera_offset_scale);
			current_zoom_fraction *= 1.f - powf(ROOT_ROOT_TWO, clicks);
			cameraOrbitIn(current_zoom_fraction *
						  mCameraOffsetDefault.length() * camera_offset_scale);
		}
		else
		{
			F32 current_zoom_fraction = (F32)mCameraFocusOffsetTarget.length();
			cameraOrbitIn(current_zoom_fraction *
						  (1.f - powf(ROOT_ROOT_TWO, clicks)));
		}
	}
}

F32 LLAgent::getCameraMinOffGround()
{
	if (mCameraMode == CAMERA_MODE_MOUSELOOK)
	{
		return 0.f;
	}
	if (noCameraConstraints())
	{
		return -1000.f;
	}
	return 0.5f;
}

void LLAgent::resetCamera()
{
	// Remove any pitch from the avatar
	LLVector3 at = mFrameAgent.getAtAxis();
	at.mV[VZ] = 0.f;
	at.normalize();
	resetAxes(at);
	// Have to explicitly clear field of view zoom now
	mCameraFOVZoomFactor = 0.f;

	updateCamera();
}

bool LLAgent::changeCameraToMouselook(bool animate)
{
	if (!gViewerWindowp)
	{
		return false;
	}
	if (LLViewerJoystick::getInstance()->getOverrideCamera()
//MK
		&& (!gRLenabled || gRLInterface.mCamDistMax > 0.f))
//mk
	{
		return false;
	}

	// Visibility changes at end of animation
	gWindowp->resetBusyCount();

	// Menus should not remain open on switching to mouselook...
	gMenuHolderp->hideMenus();

	// Unpause avatar animation
	mPauseRequest = NULL;

	gToolMgr.setCurrentToolset(gMouselookToolset);

	if (LLFloaterTools::isVisible())
	{
		gFloaterToolsp->close();
	}

	// Reset the view to rear view.
	setupCameraView(true);

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->stopMotion(ANIM_AGENT_BODY_NOISE);
		gAgentAvatarp->stopMotion(ANIM_AGENT_BREATHE_ROT);
	}

#if 0
	gViewerWindowp->stopGrab();
#endif
	gSelectMgr.deselectAll();
	gViewerWindowp->hideCursor();
	gViewerWindowp->moveCursorToCenter();

	if (mCameraMode != CAMERA_MODE_MOUSELOOK)
	{
		gFocusMgr.setKeyboardFocus(NULL);

		mLastCameraMode = mCameraMode;
		mCameraMode = CAMERA_MODE_MOUSELOOK;
		U32 old_flags = mControlFlags;
		setControlFlags(AGENT_CONTROL_MOUSELOOK);
		if (old_flags != mControlFlags)
		{
			mFlagsDirty = true;
		}

		if (animate)
		{
			startCameraAnimation();
		}
		else
		{
			mCameraAnimating = false;
			endAnimationUpdateUI();
		}
		gViewerWindowp->resetMouselookFadeTimer();

		if (gAutomationp)
		{
			gAutomationp->onCameraModeChange(mCameraMode);
		}
	}

	return true;
}

bool LLAgent::changeCameraToDefault(bool animate)
{
	if (LLViewerJoystick::getInstance()->getOverrideCamera()
//MK
		&& (!gRLenabled || gRLInterface.mCamDistMax > 0.f))
//mk
	{
		return false;
	}

	if (LLFollowCamMgr::getActiveFollowCamParams())
	{
		return changeCameraToFollow(animate);
	}

	return changeCameraToThirdPerson(animate);
}

bool LLAgent::changeCameraToFollow(bool animate)
{
	if (LLViewerJoystick::getInstance()->getOverrideCamera()
//MK
		&& (!gRLenabled || gRLInterface.mCamDistMax > 0.f))
//mk
	{
		return false;
	}

	if (mCameraMode != CAMERA_MODE_FOLLOW)
	{
		if (mCameraMode == CAMERA_MODE_MOUSELOOK)
		{
			animate = false;
		}
		startCameraAnimation();

		mLastCameraMode = mCameraMode;
		mCameraMode = CAMERA_MODE_FOLLOW;

		// Bang-in the current focus, position, and up vector of the follow cam
		mFollowCam.reset(mCameraPositionAgent,
						 gViewerCamera.getPointOfInterest(),
						 LLVector3::z_axis);

		if (gBasicToolset)
		{
			gToolMgr.setCurrentToolset(gBasicToolset);
		}

		if (isAgentAvatarValid())
		{
			gAgentAvatarp->mPelvisp->setPosition(LLVector3::zero);
			gAgentAvatarp->startMotion(ANIM_AGENT_BODY_NOISE);
			gAgentAvatarp->startMotion(ANIM_AGENT_BREATHE_ROT);
		}

		if (LLFloaterTools::isVisible())
		{
			gFloaterToolsp->close();
		}

		// Unpause avatar animation
		mPauseRequest = NULL;

		clearControlFlags(AGENT_CONTROL_MOUSELOOK);

		if (animate)
		{
			startCameraAnimation();
		}
		else
		{
			mCameraAnimating = false;
			endAnimationUpdateUI();
		}

		if (gAutomationp)
		{
			gAutomationp->onCameraModeChange(mCameraMode);
		}
	}

	return true;
}

bool LLAgent::changeCameraToThirdPerson(bool animate)
{
	if (!gViewerWindowp)
	{
		return false;
	}

	if (LLViewerJoystick::getInstance()->getOverrideCamera()
//MK
		&& (!gRLenabled || gRLInterface.mCamDistMax > 0.f))
//mk
	{
		return false;
	}

	gWindowp->resetBusyCount();

	mCameraZoomFraction = INITIAL_ZOOM_FRACTION;

	if (isAgentAvatarValid())
	{
		if (!gAgentAvatarp->mIsSitting)
		{
			gAgentAvatarp->mPelvisp->setPosition(LLVector3::zero);
		}
		gAgentAvatarp->startMotion(ANIM_AGENT_BODY_NOISE);
		gAgentAvatarp->startMotion(ANIM_AGENT_BREATHE_ROT);
	}

	// Unpause avatar animation
	mPauseRequest = NULL;

	if (mCameraMode != CAMERA_MODE_THIRD_PERSON)
	{
		if (gBasicToolset)
		{
			gToolMgr.setCurrentToolset(gBasicToolset);
		}

		if (LLFloaterTools::isVisible())
		{
			gFloaterToolsp->close();
		}

		mCameraLag.clear();
		if (mCameraMode == CAMERA_MODE_MOUSELOOK)
		{
			mCurrentCameraDistance = MIN_CAMERA_DISTANCE;
			mTargetCameraDistance = MIN_CAMERA_DISTANCE;
			animate = false;
		}
		mLastCameraMode = mCameraMode;
		mCameraMode = CAMERA_MODE_THIRD_PERSON;
		clearControlFlags(AGENT_CONTROL_MOUSELOOK);

		if (gAutomationp)
		{
			gAutomationp->onCameraModeChange(mCameraMode);
		}
	}

	LLViewerObject* parentp = NULL;
	if (isAgentAvatarValid())
	{
		parentp = (LLViewerObject*)gAgentAvatarp->getParent();
	}
	// Remove any pitch from the avatar
	if (parentp)
	{
		LLVector3 at_axis = gViewerCamera.getAtAxis();
		at_axis.mV[VZ] = 0.f;
		at_axis.normalize();
		LLQuaternion obj_rot = parentp->getRenderRotation();
		resetAxes(at_axis * ~obj_rot);
	}
	else
	{
		LLVector3 at_axis = mFrameAgent.getAtAxis();
		at_axis.mV[VZ] = 0.f;
		at_axis.normalize();
		resetAxes(at_axis);
	}

	if (animate)
	{
		startCameraAnimation();
	}
	else
	{
		mCameraAnimating = false;
		endAnimationUpdateUI();
	}

//MK
	if (gRLenabled && gRLInterface.mCamDistMax <= 0.f)
	{
		// Make sure we stay in mouselook
		changeCameraToMouselook(false);
		return false;
	}
//mk

	return true;
}

void LLAgent::changeCameraToCustomizeAvatar()
{
	if (!gViewerWindowp)
	{
		return;
	}

	if (LLViewerJoystick::getInstance()->getOverrideCamera()
//MK
		&& (!gRLenabled || gRLInterface.mCamDistMax > 0.f))
//mk
	{
		return;
	}

	bool animate = gSavedSettings.getBool("AppearanceAnimation");
//MK
	if (animate && gRLenabled && gAgentAvatarp->mIsSitting &&
		(gRLInterface.mContainsUnsit || gRLInterface.mContainsStandtp))
	{
		// We are not allowed to stand up, so do not animate !
		animate = false;
	}
//mk
	if (animate && gAgentAvatarp->mIsSitting)
	{
		// Force stand up
		LL_DEBUGS("AgentSit") << "Sending agent unsit request" << LL_ENDL;
		setControlFlags(AGENT_CONTROL_STAND_UP);
	}

	gWindowp->resetBusyCount();

	if (LLFloaterTools::isVisible())
	{
		gFloaterToolsp->close();
	}

	gToolMgr.setCurrentToolset(gFaceEditToolset);

	if (mCameraMode != CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		startCameraAnimation();

		if (animate || gSavedSettings.getBool("AppearanceCameraMovement"))
		{
			setupCameraView(true);	// Reset the view to rear view.
		}
		mLastCameraMode = mCameraMode;
		mCameraMode = CAMERA_MODE_CUSTOMIZE_AVATAR;
		clearControlFlags(AGENT_CONTROL_MOUSELOOK);

		gFocusMgr.setKeyboardFocus(NULL);
		gFocusMgr.setMouseCapture(NULL);

		if (animate)
		{
			// Remove any pitch or rotation from the avatar
			LLVector3 at = mFrameAgent.getAtAxis();
			at.mV[VZ] = 0.f;
			at.normalize();
			resetAxes(at);

			sendAnimationRequest(ANIM_AGENT_CUSTOMIZE, ANIM_REQUEST_START);
			mCustomAnim = true;
			gAgentAvatarp->startMotion(ANIM_AGENT_CUSTOMIZE);
			LLMotion* turn_motion =
				gAgentAvatarp->findMotion(ANIM_AGENT_CUSTOMIZE);
			if (turn_motion)
			{
				mAnimationDuration = turn_motion->getDuration() +
									 CUSTOMIZE_AVATAR_CAMERA_ANIM_SLOP;
			}
			else
			{
				mAnimationDuration = gSavedSettings.getF32("ZoomTime");
			}
		}

		setFocusGlobal(LLVector3d::zero);

		if (gAutomationp)
		{
			gAutomationp->onCameraModeChange(mCameraMode);
		}
	}
}

//
// Focus point management
//

void LLAgent::startCameraAnimation()
{
	mAnimationCameraStartGlobal = getCameraPositionGlobal();
	mAnimationFocusStartGlobal = mFocusGlobal;
	mAnimationTimer.reset();
	mCameraAnimating = true;
	static LLCachedControl<F32> zoom_time(gSavedSettings, "ZoomTime");
	mAnimationDuration = zoom_time;
}

void LLAgent::clearFocusObject()
{
	if (mFocusObject.notNull())
	{
		startCameraAnimation();

		setFocusObject(NULL);
		mFocusObjectOffset.clear();
	}
}

void LLAgent::setFocusObject(LLViewerObject* object)
{
	mFocusObject = object;
}

// Focus on a point, but try to keep camera position stable.
void LLAgent::setFocusGlobal(const LLPickInfo& pick)
{
	LLViewerObject* objectp = gObjectList.findObject(pick.mObjectID);
	if (objectp && !objectp->isRiggedMesh())
	{
		// Focus on object plus designated offset which may or may not be same
		// as pick.mPosGlobal, excepted for rigged items to prevent wrong focus
		// position
		setFocusGlobal(objectp->getPositionGlobal() +
					   LLVector3d(pick.mObjectOffset),
					   pick.mObjectID);
	}
	else
	{
		// Focus directly on point where user clicked
		setFocusGlobal(pick.mPosGlobal, pick.mObjectID);
	}
}

void LLAgent::setFocusGlobal(const LLVector3d& focus, const LLUUID& object_id)
{
	setFocusObject(gObjectList.findObject(object_id));
	LLVector3d old_focus = mFocusTargetGlobal;
	LLViewerObject* focus_obj = mFocusObject;

	if (focus.isExactlyZero())
	{
		if (isAgentAvatarValid())
		{
			mFocusTargetGlobal =
				getPosGlobalFromAgent(gAgentAvatarp->mHeadp->getWorldPosition());
		}
		else
		{
			mFocusTargetGlobal = getPositionGlobal();
		}
	}

	// If focus has changed
	if (old_focus != focus)
	{
		if (focus.isExactlyZero())
		{
			mCameraFocusOffsetTarget = getCameraPositionGlobal() -
									   mFocusTargetGlobal;
			mCameraFocusOffset = mCameraFocusOffsetTarget;
			setLookAt(LOOKAT_TARGET_CLEAR);
		}
		else
		{
			mFocusTargetGlobal = focus;
			if (!focus_obj)
			{
				mCameraFOVZoomFactor = 0.f;
			}

			mCameraFocusOffsetTarget =
				getPosGlobalFromAgent(mCameraVirtualPositionAgent) -
				mFocusTargetGlobal;

			startCameraAnimation();

			if (focus_obj)
			{
				if (focus_obj->isAvatar())
				{
					setLookAt(LOOKAT_TARGET_FOCUS, focus_obj);
				}
				else
				{
					setLookAt(LOOKAT_TARGET_FOCUS, focus_obj,
							  (getPosAgentFromGlobal(focus) -
							   focus_obj->getRenderPosition()) *
							  ~focus_obj->getRenderRotation());
				}
			}
			else
			{
				setLookAt(LOOKAT_TARGET_FOCUS, NULL,
						  getPosAgentFromGlobal(mFocusTargetGlobal));
			}
		}
	}
	else // focus == mFocusTargetGlobal
	{
		mCameraFocusOffsetTarget = (getCameraPositionGlobal() -
									mFocusTargetGlobal) /
								   (1.f + mCameraFOVZoomFactor);
		mCameraFocusOffset = mCameraFocusOffsetTarget;
	}

	if (mFocusObject.notNull())
	{
		// For attachments, make offset relative to avatar, not the attachment
		if (mFocusObject->isAttachment())
		{
			while (mFocusObject.notNull() && !mFocusObject->isAvatar())
			{
				mFocusObject = (LLViewerObject*)mFocusObject->getParent();
			}
			setFocusObject((LLViewerObject*)mFocusObject);
		}
		updateFocusOffset();
	}
}

// Used for avatar customization
void LLAgent::setCameraPosAndFocusGlobal(const LLVector3d& camera_pos,
										 const LLVector3d& focus,
										 const LLUUID& object_id)
{
	LLVector3d old_focus = mFocusTargetGlobal.isExactlyZero() ?
								focus : mFocusTargetGlobal;

	F64 focus_delta_squared = (old_focus - focus).lengthSquared();
	constexpr F64 ANIM_EPSILON_SQUARED = 0.0001;
	if (focus_delta_squared > ANIM_EPSILON_SQUARED)
	{
		startCameraAnimation();

		if (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			// Compensate for the fact that the camera has already been offset
			// to make room for LLFloaterCustomize.
			mAnimationCameraStartGlobal -=
				LLVector3d(gViewerCamera.getLeftAxis() *
						   calcCustomizeAvatarUIOffset(mAnimationCameraStartGlobal));
		}
	}

#if 0
	gViewerCamera.setOrigin(getPosAgentFromGlobal(camera_pos));
#endif
	setFocusObject(gObjectList.findObject(object_id));
	mFocusTargetGlobal = focus;
	mCameraFocusOffsetTarget = camera_pos - focus;
	mCameraFocusOffset = mCameraFocusOffsetTarget;

	if (mFocusObject)
	{
		if (mFocusObject->isAvatar())
		{
			setLookAt(LOOKAT_TARGET_FOCUS, mFocusObject);
		}
		else
		{
			setLookAt(LOOKAT_TARGET_FOCUS, mFocusObject,
					  (getPosAgentFromGlobal(focus) -
					   mFocusObject->getRenderPosition()) *
					  ~mFocusObject->getRenderRotation());
		}
	}
	else
	{
		setLookAt(LOOKAT_TARGET_FOCUS, NULL,
				  getPosAgentFromGlobal(mFocusTargetGlobal));
	}

	if (mCameraAnimating)
	{
		constexpr F64 ANIM_METERS_PER_SECOND = 10.0;
		constexpr F64 MIN_ANIM_SECONDS = 0.5;
		constexpr F64 MAX_ANIM_SECONDS = 10.0;
		F64 anim_duration = llmax(MIN_ANIM_SECONDS,
								  sqrt(focus_delta_squared) /
								  ANIM_METERS_PER_SECOND);
		anim_duration = llmin(anim_duration, MAX_ANIM_SECONDS);
		setAnimationDuration((F32)anim_duration);
	}

	updateFocusOffset();
}

void LLAgent::setSitCamera(const LLUUID& object_id,
						   const LLVector3& camera_pos,
						   const LLVector3& camera_focus)
{
	if (object_id.notNull())
	{
		LLViewerObject* reference_object = gObjectList.findObject(object_id);
		if (reference_object)
		{
			// Convert to root object relative ?
			mSitCameraPos = camera_pos;
			mSitCameraFocus = camera_focus;
			mSitCameraReferenceObject = reference_object;
			mSitCameraEnabled = true;
		}
	}
	else
	{
		mSitCameraPos.clear();
		mSitCameraFocus.clear();
		mSitCameraReferenceObject = NULL;
		mSitCameraEnabled = false;
	}
}

void LLAgent::setFocusOnAvatar(bool focus_on_avatar, bool animate)
{
	if (focus_on_avatar != mFocusOnAvatar)
	{
		if (animate)
		{
			startCameraAnimation();
		}
		else
		{
			stopCameraAnimation();
		}
	}

	if (!mFocusOnAvatar && focus_on_avatar)
	{
		setFocusGlobal(LLVector3d::zero);
		mCameraFOVZoomFactor = 0.f;
		if (mCameraMode == CAMERA_MODE_THIRD_PERSON)
		{
			LLVector3 at_axis =
				gSavedSettings.getBool("ResetViewRotatesAvatar") ?
					gViewerCamera.getAtAxis() : mFrameAgent.getAtAxis();
			at_axis.mV[VZ] = 0.f;
			at_axis.normalize();
			if (isAgentAvatarValid())
			{
				LLViewerObject* parentp =
					(LLViewerObject*)gAgentAvatarp->getParent();
				if (parentp)
				{
					LLQuaternion obj_rot = parentp->getRenderRotation();
					at_axis = at_axis * ~obj_rot;
				}
			}
			resetAxes(at_axis);
		}
	}
	// Unlocking camera from avatar
	else if (mFocusOnAvatar && !focus_on_avatar)
	{
		// Keep camera focus point consistent, even though it is now unlocked
		setFocusGlobal(getPositionGlobal() + calcThirdPersonFocusOffset(),
					   gAgentID);
	}

	mFocusOnAvatar = focus_on_avatar;
}

void LLAgent::heardChat(const LLUUID& id)
{
	// Log text and voice chat to speaker manager for keeping track of active
	// speakers, etc.
	LLLocalSpeakerMgr::getInstance()->speakerChatted(id);

	// Do not respond to our own voice
	if (id == gAgentID) return;

	if (ll_rand(2) == 0)
	{
		LLViewerObject* chatter = gObjectList.findObject(mLastChatterID);
		setLookAt(LOOKAT_TARGET_AUTO_LISTEN, chatter, LLVector3::zero);
	}

	mLastChatterID = id;
	mChatTimer.reset();
}

void LLAgent::lookAtLastChat()
{
	// Block if camera is animating or not in normal third person camera mode
	if (mCameraAnimating || !cameraThirdPerson())
	{
		return;
	}

	LLViewerObject* chatter = gObjectList.findObject(mLastChatterID);
	if (!chatter)
	{
		return;
	}

	LLVector3 delta_pos;
	if (chatter->isAvatar())
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)chatter;
		if (isAgentAvatarValid() && avatarp->mHeadp)
		{
			delta_pos = avatarp->mHeadp->getWorldPosition() -
						gAgentAvatarp->mHeadp->getWorldPosition();
		}
		else
		{
			delta_pos = chatter->getPositionAgent() - getPositionAgent();
		}
		delta_pos.normalize();

		setControlFlags(AGENT_CONTROL_STOP);

		changeCameraToThirdPerson();

		LLVector3 new_camera_pos = gAgentAvatarp->mHeadp->getWorldPosition();
		LLVector3 left = delta_pos % LLVector3::z_axis;
		left.normalize();
		LLVector3 up = left % delta_pos;
		up.normalize();
		new_camera_pos -= delta_pos * 0.4f;
		new_camera_pos += left * 0.3f;
		new_camera_pos += up * 0.2f;

		setFocusOnAvatar(false, false);

		if (avatarp->mHeadp)
		{
			setFocusGlobal(getPosGlobalFromAgent(avatarp->mHeadp->getWorldPosition()),
						   mLastChatterID);
			mCameraFocusOffsetTarget =
				getPosGlobalFromAgent(new_camera_pos) -
				getPosGlobalFromAgent(avatarp->mHeadp->getWorldPosition());
		}
		else
		{
			setFocusGlobal(chatter->getPositionGlobal(), mLastChatterID);
			mCameraFocusOffsetTarget = getPosGlobalFromAgent(new_camera_pos) -
									   chatter->getPositionGlobal();
		}
	}
	else if (!chatter->isHUDAttachment())
	{
		delta_pos = chatter->getRenderPosition() - getPositionAgent();
		delta_pos.normalize();

		setControlFlags(AGENT_CONTROL_STOP);

		changeCameraToThirdPerson();

		LLVector3 new_camera_pos = gAgentAvatarp->mHeadp->getWorldPosition();
		LLVector3 left = delta_pos % LLVector3::z_axis;
		left.normalize();
		LLVector3 up = left % delta_pos;
		up.normalize();
		new_camera_pos -= delta_pos * 0.4f;
		new_camera_pos += left * 0.3f;
		new_camera_pos += up * 0.2f;

		setFocusOnAvatar(false, false);

		setFocusGlobal(chatter->getPositionGlobal(), mLastChatterID);
		mCameraFocusOffsetTarget = getPosGlobalFromAgent(new_camera_pos) -
								   chatter->getPositionGlobal();
	}
}

void LLAgent::lookAtObject(LLUUID object_id, ECameraPosition camera_pos)
{
	// Block if camera is animating or not in normal third person camera mode
	if (mCameraAnimating || !cameraThirdPerson())
	{
		return;
	}

	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp) return;

	LLVector3 delta_pos;
	if (objectp->isAvatar())
	{
		setFocusOnAvatar(false, false);

		LLVOAvatar* avatarp = (LLVOAvatar*)objectp;
		if (isAgentAvatarValid() && avatarp->mHeadp)
		{
			delta_pos = avatarp->mHeadp->getWorldPosition() -
						gAgentAvatarp->mHeadp->getWorldPosition();
		}
		else
		{
			delta_pos = objectp->getPositionAgent() - getPositionAgent();
		}
		delta_pos.normalize();

		setControlFlags(AGENT_CONTROL_STOP);

		changeCameraToThirdPerson();

		LLVector3 new_camera_pos = gAgentAvatarp->mHeadp->getWorldPosition();
		LLVector3 left = delta_pos % LLVector3::z_axis;
		left.normalize();
		LLVector3 up = left % delta_pos;
		up.normalize();
		new_camera_pos -= delta_pos * 0.4f;
		new_camera_pos += left * 0.3f;
		new_camera_pos += up * 0.2f;

		F32 radius = avatarp->getVObjRadius();
		LLVector3d view_dist(radius, radius, 0.f);

		if (avatarp->mHeadp)
		{
			setFocusGlobal(getPosGlobalFromAgent(avatarp->mHeadp->getWorldPosition()),
												 object_id);
			mCameraFocusOffsetTarget =
				getPosGlobalFromAgent(new_camera_pos) -
				getPosGlobalFromAgent(avatarp->mHeadp->getWorldPosition());

			if (camera_pos == CAMERA_POSITION_SELF)
			{
				mCameraFocusOffsetTarget =
					getPosGlobalFromAgent(new_camera_pos) -
					getPosGlobalFromAgent(avatarp->mHeadp->getWorldPosition());
			}
			else	// CAMERA_POSITION_OBJECT
			{
				mCameraFocusOffsetTarget = view_dist;
			}
		}
		else
		{
			setFocusGlobal(objectp->getPositionGlobal(), object_id);
			mCameraFocusOffsetTarget = getPosGlobalFromAgent(new_camera_pos) -
									   objectp->getPositionGlobal();

			if (camera_pos == CAMERA_POSITION_SELF)
			{
				mCameraFocusOffsetTarget = getPosGlobalFromAgent(new_camera_pos) -
										   objectp->getPositionGlobal();
			}
			else	// CAMERA_POSITION_OBJECT
			{
				mCameraFocusOffsetTarget = view_dist;
			}
		}

		setFocusOnAvatar(false);
	}
	else if (!objectp->isHUDAttachment())
	{
		setFocusOnAvatar(false, false);

		delta_pos = objectp->getRenderPosition() - getPositionAgent();
		delta_pos.normalize();

		setControlFlags(AGENT_CONTROL_STOP);

		changeCameraToThirdPerson();

		LLVector3 new_camera_pos = gAgentAvatarp->mHeadp->getWorldPosition();
		LLVector3 left = delta_pos % LLVector3::z_axis;
		left.normalize();
		LLVector3 up = left % delta_pos;
		up.normalize();
		new_camera_pos -= delta_pos * 0.4f;
		new_camera_pos += left * 0.3f;
		new_camera_pos += up * 0.2f;

		setFocusGlobal(objectp->getPositionGlobal(), object_id);

		if (camera_pos == CAMERA_POSITION_SELF)
		{
			mCameraFocusOffsetTarget = getPosGlobalFromAgent(new_camera_pos) -
									   objectp->getPositionGlobal();
		}
		else	// CAMERA_POSITION_OBJECT
		{
			F32 radius = objectp->getVObjRadius();
			LLVector3d view_dist(radius, radius, 0.f);
			mCameraFocusOffsetTarget = view_dist;
		}

		setFocusOnAvatar(false);
	}
}

LLSD ll_sdmap_from_vector3(const LLVector3& vec)
{
	LLSD ret;
	ret["X"] = vec.mV[VX];
	ret["Y"] = vec.mV[VY];
	ret["Z"] = vec.mV[VZ];
	return ret;
}

void LLAgent::setStartPosition(U32 location_id)
{
	if (gAgentID.isNull() || !gObjectList.findAvatar(gAgentID))
	{
		llwarns << "Cannot find agent viewer object id " << gAgentID
				<< ". Operation aborted." << llendl;
		return;
	}
	if (!mRegionp)
	{
		llwarns << "Undefined agent region. Operation aborted." << llendl;
		return;
	}

	// We have got the viewer object. Sometimes the agent can be velocity
	// interpolated off of this simulator. Clamp it to the region the agent is
	// in, a little bit in on each side.
	constexpr F32 INSET = 0.5f; //meters
	const F32 region_width = mRegionp->getWidth();

	LLVector3 agent_pos = getPositionAgent();

	if (isAgentAvatarValid())
	{
		// The z height is at the agent's feet
		agent_pos.mV[VZ] -= 0.5f * (gAgentAvatarp->mBodySize.mV[VZ] +
									gAgentAvatarp->mAvatarOffset.mV[VZ]);
	}

	agent_pos.mV[VX] = llclamp(agent_pos.mV[VX], INSET, region_width - INSET);
	agent_pos.mV[VY] = llclamp(agent_pos.mV[VY], INSET, region_width - INSET);

	// Don't let them go below ground, or too high.
	agent_pos.mV[VZ] = llclamp(agent_pos.mV[VZ],
							   mRegionp->getLandHeightRegion(agent_pos),
							   MAX_OBJECT_Z);

	const std::string& url = getRegionCapability("HomeLocation");
	if (!url.empty())
	{
		// Send the capability request
		LLSD loc;
		loc["LocationId"] = LLSD::Integer(location_id);
		loc["LocationPos"] = ll_sdmap_from_vector3(agent_pos);
		loc["LocationLookAt"] = ll_sdmap_from_vector3(mFrameAgent.getAtAxis());

		LLSD body;
		body["HomeLocation"] = loc;

		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url,
															   mHttpPolicy,
															   body,
															   setStartPositionSuccess,
															   NULL);
		return;
	}

	// Old UDP message based method
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessageFast(_PREHASH_SetStartLocationRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_StartLocationData);
	// Corrected by the sim
	msg->addStringFast(_PREHASH_SimName, "");
	msg->addU32Fast(_PREHASH_LocationID, location_id);
	msg->addVector3Fast(_PREHASH_LocationPos, agent_pos);
	msg->addVector3Fast(_PREHASH_LocationLookAt, mFrameAgent.getAtAxis());

	// Reliable only helps when setting home location. Last location is sent on
	// quit, and we do not have time to ack the packets.
	msg->sendReliable(mRegionp->getHost());

	// With the old UDP method, we suppose the request to set home to here will
	// be granted... Which might not even be true !
	if (location_id == START_LOCATION_ID_HOME)
	{
		setHomePosRegion(mRegionp->getHandle(), getPositionAgent());
		std::string name = gViewerParcelMgr.getAgentParcelName() + "|" +
						   mRegionp->getName();
		LLStringUtil::trim(name);
		gSavedPerAccountSettings.setString("AgentHomeParcel", name);
	}
}

//static
void LLAgent::setStartPositionSuccess(const LLSD& result)
{
	// Check for a valid server response
	if (!result.has("success") || !result["success"].asBoolean() ||
		!result.has("HomeLocation") ||
		!result["HomeLocation"].has("LocationPos") ||
		!result["HomeLocation"]["LocationPos"].has("X") ||
		!result["HomeLocation"]["LocationPos"].has("Y") ||
		!result["HomeLocation"]["LocationPos"].has("Z"))
	{
		llwarns << "Invalid server response for home location" << llendl;
		return;
	}

	LLVector3 agent_pos;
	agent_pos.mV[VX] = result["HomeLocation"]["LocationPos"]["X"].asInteger();
	agent_pos.mV[VY] = result["HomeLocation"]["LocationPos"]["Y"].asInteger();
	agent_pos.mV[VZ] = result["HomeLocation"]["LocationPos"]["Z"].asInteger();

	LLViewerRegion* regionp = gAgent.mRegionp;
	if (regionp)
	{
		llinfos << "Setting home position." << llendl;
		gAgent.setHomePosRegion(regionp->getHandle(), agent_pos);
		std::string name = gViewerParcelMgr.getAgentParcelName() + "|" +
						   regionp->getName();
		LLStringUtil::trim(name);
		gSavedPerAccountSettings.setString("AgentHomeParcel", name);
	}
	else
	{
		llwarns << "No region for agent; disconnected ?  Aborted." << llendl;
	}
}

void LLAgent::requestStopMotion(LLMotion* motion)
{
	// Notify all avatars that a motion has stopped. This is needed to clear
	// the animation state bits
	LLUUID anim_state_id = motion->getID();
	onAnimStop(anim_state_id);

	// If motion is not looping, it could have stopped by running out of time
	// so we need to tell the server this
	sendAnimationRequest(anim_state_id, ANIM_REQUEST_STOP);
}

void LLAgent::onAnimStop(const LLUUID& id)
{
	// Handle automatic state transitions (based on completion of animation
	// playback)
	if (id == ANIM_AGENT_STAND)
	{
		stopFidget();
	}
	else if (id == ANIM_AGENT_AWAY)
	{
		clearAFK();
	}
	else if (id == ANIM_AGENT_STANDUP)
	{
		// Send stand up command
		setControlFlags(AGENT_CONTROL_FINISH_ANIM);

		// Now trigger dusting self off animation
		if (isAgentAvatarValid() && !gAgentAvatarp->mBelowWater && rand() % 3 == 0)
		{
			sendAnimationRequest(ANIM_AGENT_BRUSH, ANIM_REQUEST_START);
		}
	}
	else if (id == ANIM_AGENT_PRE_JUMP || id == ANIM_AGENT_LAND ||
			 id == ANIM_AGENT_MEDIUM_LAND)
	{
		setControlFlags(AGENT_CONTROL_FINISH_ANIM);
	}
//MK
	else if (gRLenabled && gRLInterface.mSitGroundOnStandUp &&
			 (id == ANIM_AGENT_SIT || id == ANIM_AGENT_SIT_FEMALE ||
			  id == ANIM_AGENT_SIT_GENERIC || id == ANIM_AGENT_SIT_TO_STAND))
	{
		// We are now standing up from an object, if we did this following a
		// @sitground command, immediately sit down on the ground.
		gRLInterface.mSitGroundOnStandUp = false;
		gAgent.setFlying(false);
		gAgent.clearControlFlags(AGENT_CONTROL_STAND_UP);
		gAgent.setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);
		gRLInterface.storeLastStandingLoc(true);
	}
//mk
}

bool LLAgent::wantsPGOnly() const
{
	return (prefersPG() || isTeen()) && !isGodlike();
}

bool LLAgent::canAccessMature() const
{
	// If you prefer mature, you're either mature or adult, and therefore can
	// access all mature content
	return isGodlike() || (prefersMature() && !isTeen());
}

bool LLAgent::canAccessAdult() const
{
	// If you prefer adult, you must BE adult.
	return isGodlike() || (prefersAdult() && isAdult());
}

// Note: this is for now only used in llviewermessage.cpp to allow playing the
// TP sound whenever we can TP to a region. In case of a more "serious" usage
// in the future, we might want to implement the "TODO" item... HB
bool LLAgent::canAccessMaturityInRegion(U64 region_handle) const
{
	LLViewerRegion* regionp = gWorld.getRegionFromHandle(region_handle);
	if (!regionp)
	{
		// Region not yet connected and instance not yet created in the viewer:
		// its maturity rating is unknown, just yet... *TODO: use sim info from
		// the world map, if available ?  HB
		return true;
	}
	U8 access = regionp->getSimAccess();
	if (access == SIM_ACCESS_MATURE && !canAccessMature())
	{
		return false;
	}
	if (access == SIM_ACCESS_ADULT && !canAccessAdult())
	{
		return false;
	}
	return true;
}

bool LLAgent::canAccessMaturityAtGlobal(LLVector3d pos_global) const
{
	U64 region_handle = to_region_handle_global(pos_global.mdV[0],
												pos_global.mdV[1]);
	return canAccessMaturityInRegion(region_handle);
}

bool LLAgent::prefersPG() const
{
	static LLCachedControl<U32> maturity(gSavedSettings, "PreferredMaturity");
	return maturity < SIM_ACCESS_MATURE;
}

bool LLAgent::prefersMature() const
{
	static LLCachedControl<U32> maturity(gSavedSettings, "PreferredMaturity");
	return maturity >= SIM_ACCESS_MATURE;
}

bool LLAgent::prefersAdult() const
{
	static LLCachedControl<U32> maturity(gSavedSettings, "PreferredMaturity");
	return maturity >= SIM_ACCESS_ADULT;
}

void LLAgent::setTeen(bool teen)
{
	mAccess = teen ? SIM_ACCESS_PG : SIM_ACCESS_MATURE;
}

//static
U8 LLAgent::convertTextToMaturity(char text)
{
	if ('A' == text)
	{
		return SIM_ACCESS_ADULT;
	}
	else if ('M' == text)
	{
		return SIM_ACCESS_MATURE;
	}
	else if ('P' == text)
	{
		return SIM_ACCESS_PG;
	}
	return SIM_ACCESS_MIN;
}

bool LLAgent::sendMaturityPreferenceToServer(U8 maturity)
{
	// Update agent access preference on the server
	const std::string url = getRegionCapability("UpdateAgentInformation");
	if (url.empty()) return false;

	// Set new access preference
	std::string matstr = LLViewerRegion::accessToShortString(maturity);
	LLSD access_prefs = LLSD::emptyMap();
	access_prefs["max"] = matstr;

	LLSD body = LLSD::emptyMap();
	body["access_prefs"] = access_prefs;
	llinfos << "Sending access prefs update to "
			<< (access_prefs["max"].asString()) << " via capability to: "
			<< url << llendl;

	httpCallback_t cbsucc = boost::bind(&LLAgent::processMaturityPreferenceFromServer,
										_1, matstr);
	httpCallback_t cbfail = boost::bind(&LLAgent::handlePreferredMaturityError,
										maturity);
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, mHttpPolicy,
														   body, cbsucc,
														   cbfail);
	return true;
}

//static
void LLAgent::processMaturityPreferenceFromServer(const LLSD& result,
												  std::string reqmatstr)
{
	std::string matstr;
	if (result.isDefined() && result.isMap() && result.has("access_prefs") &&
		result.get("access_prefs").isMap() &&
		result.get("access_prefs").has("max") &&
		result.get("access_prefs").get("max").isString())
	{
		matstr = result.get("access_prefs").get("max").asString();
		LLStringUtil::trim(matstr);
	}
	if (matstr == reqmatstr)
	{
		llinfos << "Maturity successfully set to: " << matstr << llendl;
	}
	else
	{
		llwarns << "While attempting to change maturity preference to '"
				<< reqmatstr << "', the server responded with '" << matstr
				<< llendl;
	}
#if 0	// Response ignored. *TODO: backport v3's maturity responder.
	gAgent.handlePreferredMaturityResult(matstr);
#endif
}

//static
void LLAgent::handlePreferredMaturityError(U8 requested_maturity)
{
	llwarns << "Error while attempting to change maturity preference to: "
			<< LLViewerRegion::accessToString(requested_maturity) << llendl;
	// Response ignored. *TODO: backport v3's maturity responder.
}

bool LLAgent::requestPostCapability(const char* cap_name, LLSD& data,
									httpCallback_t cbsucc,
									httpCallback_t cbfail)
{
	const std::string& url = getRegionCapability(cap_name);
	if (url.empty())
	{
		llinfos << "No region capability: " << cap_name << llendl;
		return false;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, mHttpPolicy,
														   data,
														   cbsucc, cbfail);
	return true;
}

bool LLAgent::requestGetCapability(const char* cap_name,
								   httpCallback_t cbsucc,
								   httpCallback_t cbfail)
{
	const std::string& url = getRegionCapability(cap_name);
	if (url.empty())
	{
		llinfos << "No region capability: " << cap_name << llendl;
		return false;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(url, mHttpPolicy,
														  cbsucc, cbfail);
	return true;
}

bool LLAgent::canSetMaturity(U8 maturity)
{
	if (isAdult() || isGodlike())
	{
		// Adults and "gods" can always set their Maturity level
		return true;
	}
	return maturity == SIM_ACCESS_PG ||
		   (maturity == SIM_ACCESS_MATURE && isMature());
}

void LLAgent::setMaturity(char text)
{
	mAccess = convertTextToMaturity(text);
	U8 preferred_access = (U8)gSavedSettings.getU32("PreferredMaturity");
	while (!canSetMaturity(preferred_access))
	{
		if (preferred_access == SIM_ACCESS_ADULT)
		{
			preferred_access = SIM_ACCESS_MATURE;
		}
		else
		{
			// Mature or invalid access gets set to PG
			preferred_access = SIM_ACCESS_PG;
		}
	}
	gSavedSettings.setU32("PreferredMaturity", preferred_access);
}

void LLAgent::setGodLevel(U8 god_level)
{
	mGodLevel = god_level;
	mGodLevelChangeSignal(god_level);
}

LLAgent::god_level_change_slot_t LLAgent::registerGodLevelChanageListener(god_level_change_callback_t callback)
{
	return mGodLevelChangeSignal.connect(callback);
}

bool LLAgent::validateMaturity(const LLSD& newvalue)
{
	return canSetMaturity(newvalue.asInteger());
}

void LLAgent::handleMaturity(const LLSD& newvalue)
{
	sendMaturityPreferenceToServer(newvalue.asInteger());
}

void LLAgent::buildFullname(std::string& name) const
{
	if (isAgentAvatarValid())
	{
		name = gAgentAvatarp->getFullname();
	}
}

void LLAgent::buildFullnameAndTitle(std::string& name) const
{
	if (isGroupMember())
	{
		name = mGroupTitle;
		name += ' ';
	}
	else
	{
		name.erase(0, name.length());
	}

	if (isAgentAvatarValid())
	{
		name += gAgentAvatarp->getFullname();
	}
}

bool LLAgent::isInGroup(const LLUUID& group_id, bool ignore_god_mode) const
{
	if (!ignore_god_mode && isGodlikeWithoutAdminMenuFakery())
	{
		return true;
	}

	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			return true;
		}
	}

	return false;
}

bool LLAgent::setGroup(const LLUUID& group_id)
{
	if (group_id == mGroupID)
	{
		return true;
	}

//MK
	if (gRLenabled && gRLInterface.contains("setgroup"))
	{
		return false;
	}
//mk

	if (group_id.notNull() && !isInGroup(group_id, true))
	{
		return false;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
	return true;
}

// This implementation should mirror LLAgentInfo::hasPowerInGroup
bool LLAgent::hasPowerInGroup(const LLUUID& group_id, U64 power) const
{
	if (isGodlikeWithoutAdminMenuFakery())
	{
		return true;
	}

	// GP_NO_POWERS can also mean no power is enough to grant an ability.
	if (power == GP_NO_POWERS)
	{
		return false;
	}

	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			return (mGroups[i].mPowers & power) > 0;
		}
	}

	return false;
}

bool LLAgent::hasPowerInActiveGroup(U64 power) const
{
	return mGroupID.notNull() && hasPowerInGroup(mGroupID, power);
}

U64 LLAgent::getPowerInGroup(const LLUUID& group_id) const
{
	if (isGodlike())
	{
		return GP_ALL_POWERS;
	}

	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			return mGroups[i].mPowers;
		}
	}

	return GP_NO_POWERS;
}

bool LLAgent::getGroupData(const LLUUID& group_id, LLGroupData& data) const
{
	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			data = mGroups[i];
			return true;
		}
	}
	return false;
}

S32 LLAgent::getGroupContribution(const LLUUID& group_id) const
{
	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			S32 contribution = mGroups[i].mContribution;
			return contribution;
		}
	}
	return 0;
}

bool LLAgent::setGroupContribution(const LLUUID& group_id, S32 contribution)
{
	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			mGroups[i].mContribution = contribution;
			LLMessageSystem* msg = gMessageSystemp;
			if (!msg) return false;
			msg->newMessage(_PREHASH_SetGroupContribution);
			msg->nextBlock(_PREHASH_AgentData);
			msg->addUUID(_PREHASH_AgentID, gAgentID);
			msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlock(_PREHASH_Data);
			msg->addUUID(_PREHASH_GroupID, group_id);
			msg->addS32(_PREHASH_Contribution, contribution);
			sendReliableMessage();
			return true;
		}
	}
	return false;
}

bool LLAgent::setUserGroupFlags(const LLUUID& group_id, bool accept_notices,
								bool list_in_profile)
{
	for (S32 i = 0, count = mGroups.size(); i < count; ++i)
	{
		if (mGroups[i].mID == group_id)
		{
			mGroups[i].mAcceptNotices = accept_notices;
			mGroups[i].mListInProfile = list_in_profile;
			LLMessageSystem* msg = gMessageSystemp;
			if (!msg) return false;
			msg->newMessage(_PREHASH_SetGroupAcceptNotices);
			msg->nextBlock(_PREHASH_AgentData);
			msg->addUUID(_PREHASH_AgentID, gAgentID);
			msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlock(_PREHASH_Data);
			msg->addUUID(_PREHASH_GroupID, group_id);
			msg->addBool(_PREHASH_AcceptNotices, accept_notices);
			msg->nextBlock(_PREHASH_NewData);
			msg->addBool(_PREHASH_ListInProfile, list_in_profile);
			sendReliableMessage();

			update_group_floaters(group_id);

			return true;
		}
	}
	return false;
}

void LLAgent::updateLanguage()
{
	LLSD body;
	body["language"] = LLUI::getLanguage();
	body["language_is_public"] = gSavedSettings.getBool("LanguageIsPublic");
	if (!requestPostCapability("UpdateAgentLanguage", body))
	{
		llwarns << "Cannot post language choice to server." << llendl;
	}
}

// Utility to build a location string
void LLAgent::buildLocationString(std::string& str)
{
	const LLVector3& agent_pos_region = getPositionAgent();
	S32 pos_x = S32(agent_pos_region.mV[VX]);
	S32 pos_y = S32(agent_pos_region.mV[VY]);
	S32 pos_z = S32(agent_pos_region.mV[VZ]);

	// Round the numbers based on the velocity
	LLVector3 agent_velocity = getVelocity();
	F32 velocity_mag_sq = agent_velocity.lengthSquared();

	constexpr F32 FLY_CUTOFF = 6.f;		// meters/sec
	constexpr F32 FLY_CUTOFF_SQ = FLY_CUTOFF * FLY_CUTOFF;
	constexpr F32 WALK_CUTOFF = 1.5f;	// meters/sec
	constexpr F32 WALK_CUTOFF_SQ = WALK_CUTOFF * WALK_CUTOFF;

	if (velocity_mag_sq > FLY_CUTOFF_SQ)
	{
		pos_x -= pos_x % 4;
		pos_y -= pos_y % 4;
	}
	else if (velocity_mag_sq > WALK_CUTOFF_SQ)
	{
		pos_x -= pos_x % 2;
		pos_y -= pos_y % 2;
	}

	// Create a defult name and description for the landmark
	std::string buffer;
	if (gViewerParcelMgr.getAgentParcelName().empty())
	{
		// The parcel does not have a name
		buffer = llformat("%.32s (%d, %d, %d)", mRegionp->getName().c_str(),
						  pos_x, pos_y, pos_z);
	}
	else
	{
		// The parcel has a name, so include it in the landmark name
		buffer = llformat("%.32s, %.32s (%d, %d, %d)",
						  gViewerParcelMgr.getAgentParcelName().c_str(),
						  mRegionp->getName().c_str(), pos_x, pos_y, pos_z);
	}
	str = buffer;
}

LLQuaternion LLAgent::getHeadRotation()
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->mPelvisp ||
		!gAgentAvatarp->mHeadp)
	{
		return LLQuaternion::DEFAULT;
	}

	if (!cameraMouselook())
	{
		return gAgentAvatarp->getRotation();
	}

	// We must be in mouselook
	LLVector3 look_dir(gViewerCamera.getAtAxis());
	LLVector3 up = look_dir % mFrameAgent.getLeftAxis();
	LLVector3 left = up % look_dir;

	LLQuaternion rot(look_dir, left, up);
	if (gAgentAvatarp->getParent())
	{
		rot = rot * ~gAgentAvatarp->getParent()->getRotation();
	}

	return rot;
}

void LLAgent::sendAnimationRequests(uuid_vec_t& anim_ids, EAnimRequest request)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (gAgentID.isNull() || !msg)
	{
		return;
	}

	msg->newMessageFast(_PREHASH_AgentAnimation);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	bool has_valid_anims = false;
	bool start_anim = request == ANIM_REQUEST_START;
	for (S32 i = 0, count = anim_ids.size(); i < count; ++i)
	{
		const LLUUID& anim_id = anim_ids[i];
		if (anim_id.notNull())
		{
			has_valid_anims = true;
			msg->nextBlockFast(_PREHASH_AnimationList);
			msg->addUUIDFast(_PREHASH_AnimID, anim_id);
			msg->addBoolFast(_PREHASH_StartAnim, start_anim);
		}
	}

	if (has_valid_anims)
	{
		msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
		msg->addBinaryDataFast(_PREHASH_TypeData, NULL, 0);
		sendReliableMessage();
	}
	else
	{
		// Nothing to send: we *must* clear the message (else, the next message
		// will retain our unsent message header, resulting in a crash in
		// LLTemplateMessageBuilder::nextBlock() at some point, due to invalid
		// block name/data).
		msg->clearMessage();
	}
}

void LLAgent::sendAnimationRequest(const LLUUID& anim_id, EAnimRequest request)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (gAgentID.isNull() || anim_id.isNull() || !mRegionp || !msg)
	{
		return;
	}

	msg->newMessageFast(_PREHASH_AgentAnimation);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlockFast(_PREHASH_AnimationList);
	msg->addUUIDFast(_PREHASH_AnimID, anim_id);
	msg->addBoolFast(_PREHASH_StartAnim, request == ANIM_REQUEST_START);

	msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
	msg->addBinaryDataFast(_PREHASH_TypeData, NULL, 0);
	sendReliableMessage();
}

// Send a message to the region to stop the NULL animation state. This will
// reset animation state overrides for the agent.
void LLAgent::sendAnimationStateReset()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg && gAgentID.notNull())
	{
		msg->newMessageFast(_PREHASH_AgentAnimation);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, LLUUID::null);
		msg->addBoolFast(_PREHASH_StartAnim, false);

		msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
		msg->addBinaryDataFast(_PREHASH_TypeData, NULL, 0);
		sendReliableMessage();
	}
}

// Send a message to the region to revoke sepecified permissions on ALL scripts
// in the region. If the target is an object in the region, permissions in
// scripts on that object are cleared. If it is the region ID, all scripts
// clear the permissions for this agent.
void LLAgent::sendRevokePermissions(const LLUUID& target, U32 permissions)
{
	// Currently, in SL, only the bits for SCRIPT_PERMISSION_TRIGGER_ANIMATION
	// and SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS are supported by the servers.
	// Sending any other bits will cause the message to be dropped without
	// changing any permission.

	LLMessageSystem* msg = gMessageSystemp;
	if (msg && gAgentID.notNull())
	{
		msg->newMessageFast(_PREHASH_RevokePermissions);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID); // Must be our ID
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ObjectID, target); // Must be in the region
		msg->addU32Fast(_PREHASH_ObjectPermissions, permissions);

		sendReliableMessage();
	}
}

void LLAgent::sendWalkRun(bool running)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		msg->newMessageFast(_PREHASH_SetAlwaysRun);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addBoolFast(_PREHASH_AlwaysRun, running);
		sendReliableMessage();
	}
}

void LLAgent::friendsChanged()
{
	LLCollectProxyBuddies collector;
	gAvatarTracker.applyFunctor(collector);
	mProxyForAgents = collector.mProxy;
}

bool LLAgent::isGrantedProxy(const LLPermissions& perm)
{
	return mProxyForAgents.count(perm.getOwner()) > 0;
}

bool LLAgent::allowOperation(PermissionBit op, const LLPermissions& perm,
							 U64 group_proxy_power, U8 god_minimum)
{
	// Check god level.
	if (getGodLevel() >= god_minimum) return true;

	if (!perm.isOwned()) return false;

	// A group member with group_proxy_power can act as owner.
	bool is_group_owned;
	LLUUID owner_id;
	perm.getOwnership(owner_id, is_group_owned);
	LLUUID group_id(perm.getGroup());
	LLUUID agent_proxy(gAgentID);

	if (is_group_owned)
	{
		if (hasPowerInGroup(group_id, group_proxy_power))
		{
			// Let the member assume the group's id for permission requests.
			agent_proxy = owner_id;
		}
	}
	// Check for granted mod permissions.
	else if (op != PERM_OWNER && isGrantedProxy(perm))
	{
		agent_proxy = owner_id;
	}

	// This is the group id to use for permission requests. Only group members
	// may use this field.
	LLUUID group_proxy;
	if (group_id.notNull() && isInGroup(group_id))
	{
		group_proxy = group_id;
	}

	// We now have max ownership information.
	if (PERM_OWNER == op)
	{
		// This this was just a check for ownership, we can now return the
		// answer.
		return (agent_proxy == owner_id);
	}

	return perm.allowOperationBy(op, agent_proxy, group_proxy);
}

void LLAgent::getName(std::string& name)
{
	name.clear();

	if (isAgentAvatarValid())
	{
		LLNameValue* first_nv = gAgentAvatarp->getNVPair("FirstName");
		LLNameValue* last_nv = gAgentAvatarp->getNVPair("LastName");
		if (first_nv && last_nv)
		{
			name = first_nv->printData() + " " + last_nv->printData();
		}
		else
		{
			llwarns << "Agent is missing FirstName and/or LastName nv pair."
					<< llendl;
		}
	}
	else
	{
		name = gLoginFirstName + " " + gLoginLastName;
	}
}

void update_group_floaters(const LLUUID& group_id)
{
	LLFloaterGroupInfo::refreshGroup(group_id);

	// Update avatar info
	LLFloaterAvatarInfo* floaterp = LLFloaterAvatarInfo::getInstance(gAgentID);
	if (floaterp)
	{
		floaterp->listAgentGroups();
	}

	if (gIMMgrp)
	{
		// Update the talk view
		gIMMgrp->refresh();
	}

	gAgent.fireEvent(new LLEvent(&gAgent, "new group"), "");
}

//static
void LLAgent::processAgentDropGroup(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Received drop group for agent other than me" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);

	// Remove the group if it already exists remove it and add the new data to
	// pick up changes.
	LLGroupData gd;
	gd.mID = group_id;
	std::vector<LLGroupData>::iterator end = gAgent.mGroups.end();
	std::vector<LLGroupData>::iterator it =
		std::find(gAgent.mGroups.begin(), end, gd);
	if (it != end)
	{
		gAgent.mGroups.erase(it);
		if (gAgent.mGroupID == group_id)
		{
			gAgent.mGroupID.setNull();
			gAgent.mGroupPowers = 0;
			gAgent.mGroupName.clear();
			gAgent.mGroupTitle.clear();
		}

		// Refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		gGroupMgr.clearGroupData(group_id);
		// Close the floater for this group, if any.
		LLFloaterGroupInfo::closeGroup(group_id);
		// Refresh the group panel of the search window, if necessary.
		HBFloaterSearch::refreshGroup(group_id);
	}
	else
	{
		llwarns << "Agent is not part of group " << group_id << llendl;
	}
}

class LLAgentDropGroupViewerNode final : public LLHTTPNode
{
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		if (!input.isMap() || !input.has("body"))
		{
			// What to do with badly formed message ?
			response->extendedResult(HTTP_BAD_REQUEST,
									 LLSD("Invalid message parameters"));
		}

		LLSD body = input["body"];
		if (body.has("body"))
		{
			// Stupid message system doubles up the "body"s
			body = body["body"];
		}

		if (body.has("AgentData") && body["AgentData"].isArray() &&
			body["AgentData"][0].isMap())
		{
			llinfos << "VALID DROP GROUP" << llendl;

			// There is only one set of data in the AgentData block
			const LLSD& agent_data = body["AgentData"][0];

			LLUUID agent_id = agent_data["AgentID"].asUUID();
			if (agent_id != gAgentID)
			{
				llwarns << "AgentDropGroup for agent other than me" << llendl;

				response->notFound();
				return;
			}

			LLUUID group_id = agent_data["GroupID"].asUUID();

			// Remove the group if it already exists remove it and add the new
			// data to pick up changes.
			LLGroupData gd;
			gd.mID = group_id;
			std::vector<LLGroupData>::iterator end = gAgent.mGroups.end();
			std::vector<LLGroupData>::iterator it =
				std::find(gAgent.mGroups.begin(), end, gd);
			if (it != end)
			{
				gAgent.mGroups.erase(it);
				if (gAgent.mGroupID == group_id)
				{
					gAgent.mGroupID.setNull();
					gAgent.mGroupPowers = 0;
					gAgent.mGroupName.clear();
					gAgent.mGroupTitle.clear();
				}

				// Refresh all group information
				gAgent.sendAgentDataUpdateRequest();

				gGroupMgr.clearGroupData(group_id);
				// Close the floater for this group, if any.
				LLFloaterGroupInfo::closeGroup(group_id);
				// Refresh the group panel of the search window, if necessary
				HBFloaterSearch::refreshGroup(group_id);
			}
			else
			{
				llwarns << "AgentDropGroup, agent is not part of group "
						<< group_id << llendl;
			}

			response->result(LLSD());
		}
		else
		{
			// What to do with badly formed message ?
			response->extendedResult(HTTP_BAD_REQUEST,
									 LLSD("Invalid message parameters"));
		}
	}
};
LLHTTPRegistration<LLAgentDropGroupViewerNode>
	gHTTPRegistrationAgentDropGroupViewerNode("/message/AgentDropGroup");

//static
void LLAgent::processAgentGroupDataUpdate(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		return;	// Not for us !... Ignore.
	}

	S32 count = msg->getNumberOfBlocksFast(_PREHASH_GroupData);
	LLGroupData group;
	bool need_floater_update = false;
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group.mID, i);
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupInsigniaID,
						 group.mInsigniaID, i);
		msg->getU64(_PREHASH_GroupData, _PREHASH_GroupPowers, group.mPowers,
					i);
		msg->getBool(_PREHASH_GroupData, _PREHASH_AcceptNotices,
					 group.mAcceptNotices, i);
		msg->getS32(_PREHASH_GroupData, _PREHASH_Contribution,
					group.mContribution, i);
		msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupName,
						   group.mName, i);

		if (group.mID.notNull())
		{
			need_floater_update = true;
			// Remove the group if it already exists and add the new data to
			// pick up changes.
			std::vector<LLGroupData>::iterator end = gAgent.mGroups.end();
			std::vector<LLGroupData>::iterator it =
				std::find(gAgent.mGroups.begin(), end, group);
			if (it != end)
			{
				gAgent.mGroups.erase(it);
			}
			gAgent.mGroups.emplace_back(group);
		}
		if (need_floater_update)
		{
			update_group_floaters(group.mID);
		}
	}
}

class LLAgentGroupDataUpdateViewerNode final : public LLHTTPNode
{
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLSD body = input["body"];
		if (body.has("body"))
		{
			body = body["body"];
		}

		LLUUID agent_id = body["AgentData"][0]["AgentID"].asUUID();
		if (agent_id != gAgentID)
		{
			llwarns << "Received agent group data update for agent other than me"
					<< llendl;
			return;
		}

		const LLSD& group_data = body["GroupData"];
		S32 group_idx = 0;
		for (LLSD::array_const_iterator it = group_data.beginArray(),
										end = group_data.endArray();
			 it != end; ++it)
		{
			LLGroupData group;
			group.mID = (*it)["GroupID"].asUUID();
			group.mPowers = ll_U64_from_sd((*it)["GroupPowers"]);
			group.mAcceptNotices = (*it)["AcceptNotices"].asBoolean();
			group.mListInProfile =
				body["NewGroupData"][group_idx++]["ListInProfile"].asBoolean();
			group.mInsigniaID = (*it)["GroupInsigniaID"].asUUID();
			group.mName = (*it)["GroupName"].asString();
			group.mContribution = (*it)["Contribution"].asInteger();

			if (group.mID.notNull())
			{
				// Remove the group if it already exists and add the new data
				// to pick up changes.
				std::vector<LLGroupData>::iterator end2 = gAgent.mGroups.end();
				std::vector<LLGroupData>::iterator it2 =
					std::find(gAgent.mGroups.begin(), end2, group);
				if (it2 != end2)
				{
					gAgent.mGroups.erase(it2);
				}
				gAgent.mGroups.emplace_back(group);
				update_group_floaters(group.mID);
			}
		}
	}
};

LLHTTPRegistration<LLAgentGroupDataUpdateViewerNode >
	gHTTPRegistrationAgentGroupDataUpdateViewerNode ("/message/AgentGroupDataUpdate");

//static
void LLAgent::processAgentDataUpdate(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		return;	// Not for us !... Ignore.
	}

	msg->getStringFast(_PREHASH_AgentData, _PREHASH_GroupTitle,
					   gAgent.mGroupTitle);

	LLUUID active_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_ActiveGroupID, active_id);
	if (active_id.notNull())
	{
		gAgent.mGroupID = active_id;
		msg->getU64(_PREHASH_AgentData, _PREHASH_GroupPowers,
					gAgent.mGroupPowers);
		msg->getString(_PREHASH_AgentData, _PREHASH_GroupName,
					   gAgent.mGroupName);
	}
	else
	{
		gAgent.mGroupID.setNull();
		gAgent.mGroupPowers = 0;
		gAgent.mGroupName.clear();
	}

	update_group_floaters(active_id);
}

//static
void LLAgent::processScriptControlChange(LLMessageSystem* msg, void**)
{
	S32 block_count = msg->getNumberOfBlocks(_PREHASH_Data);
	for (S32 block_index = 0; block_index < block_count; ++block_index)
	{
		bool take_controls, passon;
		U32 controls;
		msg->getBool(_PREHASH_Data, _PREHASH_TakeControls, take_controls,
					 block_index);
		if (take_controls)
		{
			// Take controls
			msg->getU32(_PREHASH_Data, _PREHASH_Controls, controls,
						block_index);
			msg->getBool(_PREHASH_Data, _PREHASH_PassToAgent, passon,
						 block_index);
			U32 total_count = 0;
			for (U32 i = 0; i < TOTAL_CONTROLS; ++i)
			{
				if (controls & (1 << i))
				{
					if (passon)
					{
						++gAgent.mControlsTakenPassedOnCount[i];
					}
					else
					{
						++gAgent.mControlsTakenCount[i];
					}
					++total_count;
				}
			}

			// Any control taken ?  If so, might be first time.
			if (total_count > 0)
			{
				LLFirstUse::useOverrideKeys();
			}
		}
		else
		{
			// Release controls
			msg->getU32(_PREHASH_Data, _PREHASH_Controls, controls,
						block_index);
			msg->getBool(_PREHASH_Data, _PREHASH_PassToAgent, passon,
						 block_index);
			for (U32 i = 0; i < TOTAL_CONTROLS; ++i)
			{
				if (controls & (1 << i))
				{
					if (passon)
					{
						--gAgent.mControlsTakenPassedOnCount[i];
						if (gAgent.mControlsTakenPassedOnCount[i] < 0)
						{
							gAgent.mControlsTakenPassedOnCount[i] = 0;
						}
					}
					else
					{
						--gAgent.mControlsTakenCount[i];
						if (gAgent.mControlsTakenCount[i] < 0)
						{
							gAgent.mControlsTakenCount[i] = 0;
						}
					}
				}
			}
		}
	}
}

//static
void LLAgent::processAgentCachedTextureResponse(LLMessageSystem* mesgsys,
												void** user_data)
{
	if (--gAgentQueryManager.mNumPendingQueries < 0)
	{
		LL_DEBUGS("Agent") << "Negative pending queries, resetting to 0."
						   << LL_ENDL;
		gAgentQueryManager.mNumPendingQueries = 0;
	}
	else
	{
		LL_DEBUGS("Agent") << "Remaining pending queries: "
						   << gAgentQueryManager.mNumPendingQueries << LL_ENDL;
	}

	if (!isAgentAvatarValid())
	{
		llwarns << "No avatar for user in cached texture update!" << llendl;
		return;
	}

	if (gAgentAvatarp->isEditingAppearance())
	{
		// Ignore baked textures when in customize mode
		LL_DEBUGS("Agent") << "Agent in customize mode, not uploading baked textures."
						   << LL_ENDL;
		return;
	}

	S32 query_id;
	mesgsys->getS32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, query_id);

	S32 num_texture_blocks =
		mesgsys->getNumberOfBlocksFast(_PREHASH_WearableData);
	S32 num_results = 0;
	U8 texture_index;
	LLUUID texture_id;
	for (S32 texture_block = 0; texture_block < num_texture_blocks;
		 ++texture_block)
	{
		mesgsys->getUUIDFast(_PREHASH_WearableData, _PREHASH_TextureID,
							 texture_id, texture_block);
		mesgsys->getU8Fast(_PREHASH_WearableData, _PREHASH_TextureIndex,
						   texture_index, texture_block);
		if ((S32)texture_index >= TEX_NUM_INDICES)
		{
			continue;
		}

		const LLAvatarAppearanceDictionary::TextureEntry* te =
			gAvatarAppDictp->getTexture((ETextureIndex)texture_index);
		if (!te)
		{
			LL_DEBUGS("Agent") << "No texture entry found for index "
							   << (U32)texture_index  << " !!!"<< LL_ENDL;
			continue;
		}

		EBakedTextureIndex baked_index = te->mBakedTextureIndex;
		if (gAgentQueryManager.mActiveCacheQueries[baked_index] != query_id)
		{
			continue;
		}

		if (texture_id.notNull())
		{
			LL_DEBUGS("Agent") << "Received cached texture "
							   << (U32)texture_index << ": "
							   << texture_id << LL_ENDL;
			gAgentAvatarp->setCachedBakedTexture((ETextureIndex)texture_index,
												 texture_id);
#if 0
			gAgentAvatarp->setTETexture(LLVOAvatar::sBakedTextureIndices[texture_index],
										texture_id);
#endif
			gAgentQueryManager.mActiveCacheQueries[baked_index] = 0;
			++num_results;
		}
		else if ((U8)baked_index >= gAgent.mUploadedBakes)
		{
			LL_DEBUGS("Agent") << "No cache for baked index "
							   << (U32)baked_index
							   << ", which is a BoM-only bake. Ignoring."
							   << LL_ENDL;
		}
		else
		{
			// No cache of this bake. Request upload.
			LL_DEBUGS("Agent") << "No cache for baked index "
							   << (U32)baked_index
							   << ", invalidating composite to trigger rebake..."
							   << LL_ENDL;
			gAgentAvatarp->invalidateComposite(gAgentAvatarp->getLayerSet(baked_index),
											   true);
		}
	}

	llinfos << "Received cached texture response for " << num_results
			<< " textures." << llendl;

	gAgentAvatarp->updateMeshTextures();

	if (gAgentQueryManager.mNumPendingQueries <= 0)
	{
		// RN: not sure why composites are disabled at this point
		gAgentAvatarp->setCompositeUpdatesEnabled(true);
		gAgent.sendAgentSetAppearance();
	}
}

bool LLAgent::anyControlGrabbed() const
{
	for (U32 i = 0; i < TOTAL_CONTROLS; ++i)
	{
		if (mControlsTakenCount[i] > 0 || mControlsTakenPassedOnCount[i] > 0)
		{
			return true;
		}
	}
	return false;
}

void LLAgent::forceReleaseControls()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		msg->newMessage(_PREHASH_ForceScriptControlRelease);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		sendReliableMessage();
	}
}

void LLAgent::setHomePosRegion(const U64& region_handle,
							   const LLVector3& pos_region)
{
	mHaveHomePosition = true;
	mHomeRegionHandle = region_handle;
	mHomePosRegion = pos_region;
}

bool LLAgent::getHomePosGlobal(LLVector3d* pos_global)
{
	if (!mHaveHomePosition)
	{
		return false;
	}
	F32 x = 0;
	F32 y = 0;
	from_region_handle(mHomeRegionHandle, &x, &y);
	pos_global->set(x + mHomePosRegion.mV[VX], y + mHomePosRegion.mV[VY],
					mHomePosRegion.mV[VZ]);
	return true;
}

void LLAgent::clearVisualParams(void* data)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->clearVisualParamWeights();
		gAgentAvatarp->updateVisualParams();
	}
}

void LLAgent::setNearChatRadius(F32 radius)
{
	mNearChatRadius = radius;
	LLHUDEffectLookAt::updateSettings();
}

//---------------------------------------------------------------------------
// Teleport
//---------------------------------------------------------------------------

void LLAgent::setTeleportedSimHandle(const LLVector3d& pos_global)
{
	if (!pos_global.isExactlyZero())
	{
		LLSimInfo* info = gWorldMap.simInfoFromPosGlobal(pos_global);
		if (info)
		{
			mTeleportedPosGlobal = pos_global;
			mTeleportedSimHandle = info->mHandle;
			// Also force an update of the number of agents in this sim ASAP
			info->mAgentsUpdateTime = 0.0;
			LL_DEBUGS("Teleport") << "Set teleported sim handle: "
								  << mTeleportedSimHandle << ". Position: "
								  << mTeleportedPosGlobal << LL_ENDL;
			return;
		}
	}
	resetTeleportedSimHandle();
}

void LLAgent::resetTeleportedSimHandle()
{
	LL_DEBUGS("Teleport") << "Resetting teleported sim handle and position"
						  << LL_ENDL;
	mTeleportedSimHandle = 0;
	mTeleportedPosGlobal.setZero();
}

// Stuff to do on any teleport
bool LLAgent::teleportCore(const LLVector3d& pos_global)
{
	LL_DEBUGS("Teleport") << "Destination global position: " << pos_global
						  << LL_ENDL;

	if (mTeleportState != TELEPORT_NONE)
	{
		llwarns << "Attempt to teleport when already teleporting." << llendl;
		return false;
	}

	if (!mRegionp)
	{
		llwarns << "Current region undefined !" << llendl;
		return false;
	}

	// Force stand up and stop a sitting animation (if any), see MAINT-3969
	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting &&
		gAgentAvatarp->getParent())
	{
		LL_DEBUGS("AgentSit") << "Unsitting agent for TP" << LL_ENDL;
		gAgentAvatarp->getOffObject();
	}

	// Hide the land floater since it will get out of date...
	LLFloaterLand::hideInstance();

	gViewerParcelMgr.deselectLand();
	LLViewerMediaFocus::getInstance()->setFocusFace(false, NULL, 0, NULL);

	// Close all pie menus, deselect land, etc, but do not change the camera
	// until we know teleport succeeded.
	resetView(false);

	gViewerStats.incStat(LLViewerStats::ST_TELEPORT_COUNT);

	bool is_local = false;
	if (!pos_global.isExactlyZero())
	{
		F32 region_x = (F32)pos_global.mdV[VX];
		F32 region_y = (F32)pos_global.mdV[VY];
		U64 region_handle = to_region_handle_global(region_x, region_y);
		is_local = mRegionp->getHandle() == region_handle;
		LL_DEBUGS("Teleport") << "Current region handle: "
							  << mRegionp->getHandle()
							  << " - Destination region handle: "
							  << region_handle << " - Local TP = " << is_local
							  << LL_ENDL;
	}
	if (is_local)
	{
		setTeleportState(TELEPORT_LOCAL);
	}
	else
	{
		// When the event poll for the agent region is not within a safe window
		// for the TP to happen while it is active on the server side, wait for
		// sending the TP until the next poll request is started and has
		// settled. HB
		static LLCachedControl<bool> tp_race_fix(gSavedSettings,
												 "TPRaceWorkAroundInSL");
		static LLCachedControl<bool> restart_poll(gSavedSettings,
												  "TPRaceRestartPoll");
		if (gIsInSecondLife && tp_race_fix && !mRegionp->isEventPollInFlight())
		{
			llinfos << "Queuing the teleport request to let the agent region event poll fire."
					<< llendl;
			setTeleportState(TELEPORT_QUEUED);
			if (restart_poll)
			{
				// *HACK: re-launch the event poll for our region to try and
				// avoid the race condition server-side.
				const std::string& url =
					mRegionp->getCapability("EventQueueGet");
				if (!url.empty())
				{
					mRegionp->setCapability("EventQueueGet", url);
				}
			}
		}
		else
		{
			setTeleportState(TELEPORT_START);
		}

		setTeleportedSimHandle(pos_global);

		if (gSavedSettings.getBool("SpeedRez"))
		{
			F32 draw_distance = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance < draw_distance)
			{
				gSavedDrawDistance = draw_distance;
			}
			gSavedSettings.setF32("SavedRenderFarClip", gSavedDrawDistance);
			gSavedSettings.setF32("RenderFarClip", 32.f);
		}

		make_ui_sound("UISndTeleportOut");
	}

	// Let the voice client know a teleport has begun so it can leave the
	// existing channel.
#if 0	// This was breaking the case of teleporting within a single sim.
		// Backing it out for now.
	gVoiceClient.leaveChannel();
#endif

	return true;
}

class HBQueuedTeleport
{
protected:
	LOG_CLASS(HBQueuedTeleport);

public:
	enum eTPType : U32
	{
		TP_NONE,
		TP_LOCATION,
		TP_LANDMARK,
		TP_LURE,
	};

	HBQueuedTeleport()
	:	mType(TP_NONE),
		mRegionhandle(0),
		mTeleportFlags(0)
	{
	}

	void queueLocation(U64 handle, const LLVector3& pos_local,
					   const LLVector3& look_at)
	{
		mType = TP_LOCATION;
		mRegionhandle = handle;
		mPosLocal = pos_local;
		bool keep_look_at = gAgent.getTeleportKeepsLookAt();
		mLookAtAxis = keep_look_at ? gViewerCamera.getAtAxis() : look_at;
		resetGuardTimer();
	}

	void queueLandmark(const LLUUID& lm_asset_id)
	{
		mType = TP_LANDMARK;
		mLandmarkAssetId = lm_asset_id;
		resetGuardTimer();
	}

	void queueLure(const LLUUID& lure_id, U32 teleport_flags)
	{
		mType = TP_LURE;
		mLureId = lure_id;
		mTeleportFlags = teleport_flags;
		resetGuardTimer();
	}

	void fire();

	bool expired()
	{
		return mGuardTimer.hasExpired();
	}

private:
	void resetGuardTimer()
	{
		// Set the guard timer to encompass the maximum delay after which the
		// LLViewerRegion::isEventPollInFlight() call for the agent region
		// should return true.
		mGuardTimer.reset();
		mGuardTimer.setTimerExpirySec(2.f * LLEventPoll::getMargin() + 0.5f);
	}

private:
	LLUUID		mLandmarkAssetId;
	LLUUID		mLureId;
	U64			mRegionhandle;
	LLVector3	mPosLocal;
	LLVector3	mLookAtAxis;
	U32			mTeleportFlags;
	U32			mType;
	LLTimer		mGuardTimer;
};

void HBQueuedTeleport::fire()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || mType == TP_NONE) return;

	if (mType == TP_LOCATION)
	{
		LL_DEBUGS("Teleport") << "Sending TeleportLocationRequest" << LL_ENDL;
		msg->newMessage(_PREHASH_TeleportLocationRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addU64(_PREHASH_RegionHandle, mRegionhandle);
		msg->addVector3(_PREHASH_Position, mPosLocal);
		msg->addVector3(_PREHASH_LookAt, mLookAtAxis);
	}
	else if (mType == TP_LANDMARK)
	{
		// When when teleporting home, reset the camera view before requesting
		// the TP, so that the camera will point in the right direction on
		// arrival.
		if (mLandmarkAssetId.isNull())
		{
			gAgent.resetView(true, true);
		}
		LL_DEBUGS("Teleport") << "Sending TeleportLandmarkRequest" << LL_ENDL;
		msg->newMessageFast(_PREHASH_TeleportLandmarkRequest);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_LandmarkID, mLandmarkAssetId);
	}
	else if (mType == TP_LURE)
	{
		LL_DEBUGS("Teleport") << "Sending TeleportLureRequest" << LL_ENDL;
		msg->newMessageFast(_PREHASH_TeleportLureRequest);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_LureID, mLureId);
		// Note: TeleportFlags is a legacy field, now derived sim-side.
		msg->addU32(_PREHASH_TeleportFlags, mTeleportFlags);
	}
	else
	{
		llerrs << "Invalid TP request type" << llendl;
	}

	gAgent.setTeleportState(LLAgent::TELEPORT_START);
	gAgent.sendReliableMessage();

	mType = TP_NONE;
	llinfos << "Teleport request sent." << llendl;
}

static HBQueuedTeleport sQueuedTeleport;

void LLAgent::fireQueuedTeleport()
{
	static LLCachedControl<bool> tp_race_fix(gSavedSettings,
											 "TPRaceWorkAroundInSL");
	if (!gIsInSecondLife || !tp_race_fix ||
		(mRegionp && mRegionp->isEventPollInFlight()) ||
		sQueuedTeleport.expired())
	{
		sQueuedTeleport.fire();
	}
}

// lm_asset_id = LLUUID::null means teleport home
void LLAgent::teleportViaLandmark(const LLUUID& lm_asset_id)
{
	LL_DEBUGS("Teleport") << "Landmark asset Id: " << lm_asset_id << LL_ENDL;
//MK
	if (gRLenabled &&
		(!LLStartUp::isLoggedIn() ||
		 (gViewerWindowp && gViewerWindowp->getShowProgress()) ||
		 gRLInterface.contains("tplm") ||
		 (gRLInterface.mContainsUnsit &&
		  isAgentAvatarValid() && gAgentAvatarp->mIsSitting)))
	{
		return;
	}
//mk

	LLVector3d pos_global;
	if (lm_asset_id.notNull() &&
		lm_asset_id != LLFloaterWorldMap::getHomeID())
	{
		LLLandmark* landmark = gLandmarkList.getAsset(lm_asset_id);
		if (landmark)
		{
			landmark->getGlobalPos(pos_global);
		}
	}

	if (teleportCore(pos_global))
	{
		sQueuedTeleport.queueLandmark(lm_asset_id);
		if (mTeleportState != TELEPORT_QUEUED)
		{
			sQueuedTeleport.fire();
		}
	}
}

void LLAgent::teleportViaLure(const LLUUID& lure_id, bool godlike)
{
	LL_DEBUGS("Teleport") << "Lure Id: " << lure_id
						  << " - God-like: " << (godlike ? "true" : "false")
						  << LL_ENDL;
	if (teleportCore())
	{
		U32 teleport_flags = 0x0;
		if (godlike)
		{
			teleport_flags |= TELEPORT_FLAGS_VIA_GODLIKE_LURE;
			teleport_flags |= TELEPORT_FLAGS_DISABLE_CANCEL;
		}
		else
		{
			teleport_flags |= TELEPORT_FLAGS_VIA_LURE;
		}

		sQueuedTeleport.queueLure(lure_id, teleport_flags);
		if (mTeleportState != TELEPORT_QUEUED)
		{
			sQueuedTeleport.fire();
		}
	}
}

void LLAgent::teleportCancel()
{
	if (mRegionp)
	{
		// Send the message
		LLMessageSystem* msg = gMessageSystemp;
		if (!msg) return;
		msg->newMessage(_PREHASH_TeleportCancel);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		sendReliableMessage();
	}
	gTeleportDisplay = false;
	LL_DEBUGS("Teleport") << "Resetting to TELEPORT_NONE" << LL_ENDL;
	setTeleportState(TELEPORT_NONE);
	gPipeline.resetVertexBuffers();
}

void LLAgent::teleportRequest(U64 region_handle, const LLVector3d& pos_global,
							  const LLVector3& pos_local,
							  const LLVector3& look_at)
{
	LL_DEBUGS("Teleport") << "Region handle: " << region_handle
						  << " - Global position: " << pos_global
						  << " - Local position: " << pos_local
						  << " - Look-at vector: " << look_at << LL_ENDL;

	if (teleportCore(pos_global))
	{
		llinfos << "TeleportLocationRequest. Region handle: " << region_handle
				<< " - Local position: " << pos_local << llendl;

		mTeleportKeepsLookAt = look_at.isExactlyZero();
		if (mTeleportKeepsLookAt)
		{
			// Detach camera from avatar, so it keeps direction
			setFocusOnAvatar(false);
		}

		sQueuedTeleport.queueLocation(region_handle, pos_local, look_at);
		if (mTeleportState != TELEPORT_QUEUED)
		{
			sQueuedTeleport.fire();
		}
	}
}

void LLAgent::teleportViaLocation(const LLVector3d& pos_global)
{
	LL_DEBUGS("Teleport") << "Global position: " << pos_global << LL_ENDL;

//MK
	if (gRLenabled &&
		(!LLStartUp::isLoggedIn() ||
		 (gViewerWindowp && gViewerWindowp->getShowProgress()) ||
		 gRLInterface.contains("tploc") ||
		 (gRLInterface.mContainsUnsit &&
		  isAgentAvatarValid() && gAgentAvatarp->mIsSitting)))
	{
		return;
	}
//mk
	if (!mRegionp)
	{
		llwarns << "NULL region pointer. Teleport aborted." << llendl;
		return;
	}

	LLVector3 pos_local;
	F32 width = REGION_WIDTH_METERS;
	U64 handle = to_region_handle(pos_global);
	LLSimInfo* info = gWorldMap.simInfoFromHandle(handle);
	if (info)
	{
		LLVector3d region_origin = info->getGlobalOrigin();
		pos_local.set(pos_global.mdV[VX] - region_origin.mdV[VX],
					  pos_global.mdV[VY] - region_origin.mdV[VY],
					  pos_global.mdV[VZ]);
		// Variable region size support
		handle = info->getHandle();		// Actual handle
		width = mRegionp->getWidth();	// Actual width
	}
	else
	{
		// Note: when we do not know about the actual region size (which we
		// pretend here to be 256m), let the server fix the region handle and
		// local coordinates by itself. Yes, this is totally bogus and the
		// result of the dirty hack that is OpenSim variable size region... HB
		F32 region_x = pos_global.mdV[VX];
		F32 region_y = pos_global.mdV[VY];
		handle = to_region_handle_global(region_x, region_y);
		pos_local.set(fmodf(region_x, width), fmodf(region_y, width),
					  pos_global.mdV[VZ]);
	}
	LLVector3 look_at = pos_local;
	look_at.mV[VX] += look_at.mV[VX] < width * 0.5f ? 1.f : -1.f;
	teleportRequest(handle, pos_global, pos_local, look_at);
}

// Teleport to global position, but keep facing in the same direction
void LLAgent::teleportViaLocationLookAt(const LLVector3d& pos_global)
{
	LL_DEBUGS("Teleport") << "Global position: " << pos_global << LL_ENDL;

//MK
	if (gRLenabled)
	{
		// Do not perform these checks if we are automatically snapping back to
		// the last standing location
		if (!gRLInterface.mSnappingBackToLastStandingLocation)
		{
			// Cannot TP if we cannot sittp, unsit, tp to a location or when
			// the forward control is taken (and not passed), and something is
			// locked
			if (gRLInterface.contains("tploc") ||
				(forwardGrabbed() && gRLInterface.mContainsDetach) ||
				 gRLInterface.mSittpMax < EXTREMUM ||
				 (gRLInterface.mContainsUnsit &&
				  isAgentAvatarValid() && gAgentAvatarp->mIsSitting))
			{
				return;
			}
		}
	}
//mk

	U64 handle = to_region_handle(pos_global);

//MK
	// If we are teleporting within the region (local teleport), check @tplocal
	if (gRLenabled && handle == to_region_handle(getPositionGlobal()))
	{
		LLVector3d pos_relative =
			(LLVector3d)(pos_global - getPositionGlobal());
		if (pos_relative.length() > gRLInterface.mTplocalMax)
		{
			return;
		}
	}
//mk

	LLVector3 pos_local;
	LLSimInfo* info = gWorldMap.simInfoFromHandle(handle);
	if (info)
	{
		// Variable region size support
		handle = info->getHandle();	// Actual handle
		pos_local.set(pos_global - from_region_handle(handle));	// Actual pos
	}
	else
	{
		// Note: when we do not know about the actual region size (which we
		// pretend here to be 256m), let the server fix the region handle and
		// local coordinates by itself. Yes, this is totally bogus and the
		// result of the dirty hack that is OpenSim variable size region... HB
		F32 region_x = pos_global.mdV[VX];
		F32 region_y = pos_global.mdV[VY];
		handle = to_region_handle_global(region_x, region_y);
		pos_local.set(fmod(region_x, REGION_WIDTH_METERS),
					  fmod(region_y, REGION_WIDTH_METERS),
					  (F32)pos_global.mdV[VZ]);
	}
	teleportRequest(handle, pos_global, pos_local);
}

void LLAgent::setTeleportState(ETeleportState state, const std::string& reason)
{
	ETeleportState old_state = mTeleportState;
	mTeleportState = state;

	if (state > TELEPORT_NONE && LLPipeline::sFreezeTime)
	{
		LLFloaterSnapshot::hide(NULL);
	}

	switch (state)
	{
		case TELEPORT_NONE:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_NONE.";
			if (!reason.empty())
			{
				LL_CONT << " Reason: " << reason;
			}
			LL_CONT << LL_ENDL;
			mTeleportKeepsLookAt = false;
			// *HACK: make sure we refresh objects visibility when we jumped
			// in position by a distance greater than the draw distance in the
			// same simulator (different simulators case is already dealt with
			// in LLVOAvatarSelf::updateRegion()). HB
			// NOTE: 'reason' is empty when the TP succeeded, so by checking it
			// we are sure that this call is not the result of a failed TP
			// (where distance == 0.f, just like a successful LM TPs). HB
			if (mArrivalHandle == mDepartureHandle && reason.empty() &&
				// Exclude the login case and spurious TELEPORT_NONE. HB
				!mPosGlobalTPdeparture.isNull())
			{
				static LLCachedControl<F32> draw_distance(gSavedSettings,
														  "RenderFarClip");
				F32 distance = (mPosGlobalTPdeparture -
								getPositionGlobal()).lengthSquared();
				LL_DEBUGS("Teleport") << "Local teleport distance: "
									  << (S32)sqrtf(distance) << "m"
									  << LL_ENDL;
				if (distance > (F32)(draw_distance * draw_distance) ||
					// Special case for a successful TP via landmark from the
					// same sim; the actual agent position is then received
					// much later from the sim, so we saddly cannot check the
					// travelled distance in this case...
					distance == 0.f)
				{
					schedule_objects_visibility_refresh(4);
				}
			}
			// Reset, in case we get spurious TELEPORT_NONE later... HB
			mPosGlobalTPdeparture.setZero();
			break;

		case TELEPORT_START:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_START"
								  << LL_ENDL;
			// Remember we started the TP process at this position. HB
			mPosGlobalTPdeparture = getPositionGlobal();
			// Store the departure region URL.
			mTeleportSourceSLURL = getSLURL();
			// Store the departure region handle
			mDepartureHandle = getRegionHandle();
			// Make sure these are equal on TP start
			mArrivalHandle = mDepartureHandle;
			// Enable the TP progress screen
			gTeleportDisplay = true;
			break;

		case TELEPORT_REQUESTED:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_REQUESTED"
								  << LL_ENDL;
			break;

		case TELEPORT_MOVING:
			// TELEPORT_MOVING is set before we arrive in the new region, but
			// after we got the destination region handle and host (so we are
			// sure, at this point, that the TP is actually possible and in
			// progress).
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_MOVING"
								  << LL_ENDL;
			resetTeleportedSimHandle();
			break;

		case TELEPORT_START_ARRIVAL:
			// TELEPORT_START_ARRIVAL is set just as we are arriving in the new
			// region and at this point setRegion() has been called (with both
			// the arrival and departure sim handles properly set).
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_START_ARRIVAL"
								  << LL_ENDL;
			if (mArrivalHandle != mDepartureHandle)
			{
#if !LL_PENDING_MESH_REQUEST_SORTING
				if (gSavedSettings.getBool("DelayPendingMeshFetchesOnTP"))
				{
					LL_DEBUGS("Teleport") << "Delaying pending mesh fetches"
										  << LL_ENDL;
					gMeshRepo.delayCurrentRequests();
				}
#endif
				if (gSavedSettings.getBool("ClearStaleTextureFetchesOnTP"))
				{
					LL_DEBUGS("Teleport") << "Clearing old texture fetches"
										  << LL_ENDL;
					// Clear old texture fetches, rebuild groups and old images
					gTextureList.clearFetchingRequests();
					gPipeline.clearRebuildGroups();
					gTextureList.flushOldImages();
					// To force-release the freed memory to the OS
					LLMemory::updateMemoryInfo(true);
				}
				LLViewerTexture::resetLowMemCondition(true);
				// Used to boost texture fetches after far TPs
				LLViewerTextureList::sLastTeleportTime = gFrameTimeSeconds;
			}
			break;

		case TELEPORT_ARRIVING:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_ARRIVING"
								  << LL_ENDL;
			// In case of a race condition between TELEPORT_START and
			// TELEPORT_MOVING:
			resetTeleportedSimHandle();

			gTextureList.mForceResetTextureStats = true;
			resetView(true, true);
			// Let the interested parties know we have teleported.
			gViewerParcelMgr.onTeleportFinished(false, getPositionGlobal());
			// Remove focus from any floater to allow moving around with keys
			// on arrival. HB
			gFocusMgr.setKeyboardFocus(NULL);
			break;

		case TELEPORT_LOCAL:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_LOCAL"
								  << LL_ENDL;
			// Remember we started the TP process at this position. HB
			mPosGlobalTPdeparture = getPositionGlobal();
			mDepartureHandle = getRegionHandle();
			mArrivalHandle = mDepartureHandle;
			resetTeleportedSimHandle();
			// Remove focus from any floater to allow moving around with keys
			// on arrival. HB
			gFocusMgr.setKeyboardFocus(NULL);
			break;

		case TELEPORT_QUEUED:
			LL_DEBUGS("Teleport") << "Switched to state TELEPORT_QUEUED"
								  << LL_ENDL;
			// Enable the TP progress screen
			gTeleportDisplay = true;
	}

	gViewerStats.resetAvatarStats();

	if (old_state != state && gAutomationp)
	{
		gAutomationp->onTPStateChange(state, reason);
	}
}

// Stops all current overriding animations on this avatar, propagating this
// change back to the server.
void LLAgent::stopCurrentAnimations()
{
	if (isAgentAvatarValid())
	{
		uuid_vec_t anim_ids;
		for (LLVOAvatar::anim_it_t
				it = gAgentAvatarp->mPlayingAnimations.begin(),
				end = gAgentAvatarp->mPlayingAnimations.end();
			 it != end; ++it)
		{
			const LLUUID& id = it->first;
			// Do not cancel a ground-sit anim, as viewers use this animation's
			// status in determining whether we are sitting.
			if (id != ANIM_AGENT_SIT_GROUND_CONSTRAINED)
			{
				// Stop this animation locally
				gAgentAvatarp->stopMotion(id, true);
				// ...and ask to the server to tell everyone.
				anim_ids.emplace_back(id);
			}
		}

		sendAnimationRequests(anim_ids, ANIM_REQUEST_STOP);

		if (gSavedSettings.getBool("ResetAnimOverrideOnStopAnimation"))
		{
			// Tell the region to clear any animation state overrides
			sendAnimationStateReset();
		}

		// Revoke all animation permissions
		if (mRegionp && gSavedSettings.getBool("RevokePermsOnStopAnimation"))
		{
			U32 permissions =
				LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TRIGGER_ANIMATION] |
				LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS];
			sendRevokePermissions(mRegionp->getRegionID(), permissions);
			if (gAgentAvatarp->mIsSitting)
			{
				// Also stand up, since auto-granted sit animation permission
				// has been revoked
				LL_DEBUGS("AgentSit") << "Sending agent unsit request"
									  << LL_ENDL;
				setControlFlags(AGENT_CONTROL_STAND_UP);
			}
		}

		// Re-assert at least the default standing animation because viewers
		// get confused by avatars without associated anims.
		sendAnimationRequest(ANIM_AGENT_STAND, ANIM_REQUEST_START);
	}
}

void LLAgent::fidget()
{
	F32 cur_time = mFidgetTimer.getElapsedTimeF32();
	if (cur_time < mNextFidgetTime || getAFK())
	{
		return;
	}
	// Calculate next fidget time
	mNextFidgetTime = cur_time + MIN_FIDGET_TIME +
					  ll_frand(MAX_FIDGET_TIME - MIN_FIDGET_TIME);

	// Pick a random fidget anim here
	S32 old_fidget = mCurrentFidget;
	mCurrentFidget = ll_rand(NUM_AGENT_STAND_ANIMS);
	if (mCurrentFidget == old_fidget)
	{
		return;
	}

	stopFidget();

	switch (mCurrentFidget)
	{
		case 0:
			break;

		case 1:
			sendAnimationRequest(ANIM_AGENT_STAND_1, ANIM_REQUEST_START);
			break;

		case 2:
			sendAnimationRequest(ANIM_AGENT_STAND_2, ANIM_REQUEST_START);
			break;

		case 3:
			sendAnimationRequest(ANIM_AGENT_STAND_3, ANIM_REQUEST_START);
			break;

		case 4:
			sendAnimationRequest(ANIM_AGENT_STAND_4, ANIM_REQUEST_START);
	}
}

//static
void LLAgent::stopFidget()
{
	uuid_vec_t anims;
	anims.emplace_back(ANIM_AGENT_STAND_1);
	anims.emplace_back(ANIM_AGENT_STAND_2);
	anims.emplace_back(ANIM_AGENT_STAND_3);
	anims.emplace_back(ANIM_AGENT_STAND_4);

	gAgent.sendAnimationRequests(anims, ANIM_REQUEST_STOP);
}

void LLAgent::requestEnterGodMode()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessageFast(_PREHASH_RequestGodlikePowers);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_RequestBlock);
	msg->addBoolFast(_PREHASH_Godlike, true);
	msg->addUUIDFast(_PREHASH_Token, LLUUID::null);

	// Simulators need to know about your request
	sendReliableMessage();
}

void LLAgent::requestLeaveGodMode()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessageFast(_PREHASH_RequestGodlikePowers);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_RequestBlock);
	msg->addBoolFast(_PREHASH_Godlike, false);
	msg->addUUIDFast(_PREHASH_Token, LLUUID::null);

	// Simulator needs to know about your request
	sendReliableMessage();
}

void LLAgent::sendAgentSetAppearance()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || !isAgentAvatarValid() || gAgentWearables.isSettingOutfit() ||
		LLVOAvatarSelf::canUseServerBaking() ||
		(gAgentQueryManager.mNumPendingQueries > 0 &&
		 !gAgentAvatarp->isEditingAppearance()))
	{
		return;
	}

#if 0
	// *FIXME: At this point we have a complete appearance to send and are in a
	// non-baking region.
	gAgentAvatarp->setIsUsingServerBakes(false);
#endif

	S32 sb_count, host_count, both_count, neither_count;
	gAgentAvatarp->bakedTextureOriginCounts(sb_count, host_count, both_count,
											neither_count);
	if (both_count != 0 || neither_count != 0)
	{
		llwarns << "Bad bake texture state. Baked count: " << sb_count
				<< " - Host count: " << host_count
				<< " - Both count: " << both_count
				<< " - Neither count: " << neither_count << llendl;
	}
	if (sb_count != 0 && host_count == 0)
	{
		gAgentAvatarp->setIsUsingServerBakes(true);
	}
	else if (sb_count == 0 && host_count != 0)
	{
		gAgentAvatarp->setIsUsingServerBakes(false);
	}
	else if (sb_count + host_count > 0)
	{
		llwarns << "Unclear baked texture state: not sending appearance."
				<< llendl;
		return;
	}

	llinfos << "TAT: Sent AgentSetAppearance: "
			<< gAgentAvatarp->getBakedStatusForPrintout() << llendl;

	msg->newMessageFast(_PREHASH_AgentSetAppearance);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	// Correct for the collision tolerance (to make it look like the agent is
	// actually walking on the ground/object).
	LLVector3 body_size = gAgentAvatarp->mBodySize;
	body_size.mV[VZ] += gSavedSettings.getF32("AvatarOffsetZ");
	body_size += gAgentAvatarp->mAvatarOffset;
	msg->addVector3Fast(_PREHASH_Size, body_size);

	// To guard against out of order packets.
	// Note: always start by sending 1. This resets the server's count. 0 on
	// the server means "uninitialized"
	msg->addU32Fast(_PREHASH_SerialNum, ++mAppearanceSerialNum);

	// Is texture data current relative to wearables ?
	// KLW - TAT this will probably need to check the local queue.
	bool textures_current = gAgentAvatarp->areTexturesCurrent();

	bool wearing_skirt =
		gAgentAvatarp->isWearingWearableType(LLWearableType::WT_SKIRT);
	bool wearing_universal =
		gAgentAvatarp->isWearingWearableType(LLWearableType::WT_UNIVERSAL);
	for (U8 i = 0; i < mUploadedBakes; ++i)
	{
		const ETextureIndex texture_index =
			LLAvatarAppearanceDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)i);

		// If we are not wearing a skirt, we do not need its texture baked
		if (texture_index == TEX_SKIRT_BAKED && !wearing_skirt)
		{
			continue;
		}
		// If we are not wearing an universal, we do not need the corresponding
		// textures baked 
		if (!wearing_universal && texture_index >= TEX_LEFT_ARM_BAKED &&
			texture_index <= TEX_AUX3_BAKED)
		{
			continue;
		}
		
		// IMG_DEFAULT_AVATAR means not baked. 0 index should be ignored for 
		// baked textures
		if (!gAgentAvatarp->isTextureDefined(texture_index, 0))
		{
			LL_DEBUGS("Avatar") << "Texture not current for baked: " << i
								<< " - local: " << (S32)texture_index
								<< LL_ENDL;
			textures_current = false;
			break;
		}
	}

	// Only update cache entries if we have all our baked textures.
	// *FIXME: need additional check for not in appearance editing mode, if
	// still using local composites need to set using local composites to
	// false, and update mesh textures.
	if (textures_current)
	{
		llinfos << "TAT: Sending cached texture data" << llendl;
		for (U8 i = 0; i < mUploadedBakes; ++i)
		{
			bool generate_valid_hash = true;
			if (!gAgentAvatarp->isBakedTextureFinal((EBakedTextureIndex)i))
			{
				generate_valid_hash = false;
				llinfos << "Not caching baked texture upload for " << i
						<< " due to being uploaded at low resolution."
						<< llendl;
			}

			const LLUUID hash =
				gAgentWearables.computeBakedTextureHash((EBakedTextureIndex)i,
														generate_valid_hash);
			if (hash.notNull())
			{
				ETextureIndex texture_index =
					LLAvatarAppearanceDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)i);
				msg->nextBlockFast(_PREHASH_WearableData);
				msg->addUUIDFast(_PREHASH_CacheID, hash);
				msg->addU8Fast(_PREHASH_TextureIndex, (U8)texture_index);
			}
		}
		msg->nextBlockFast(_PREHASH_ObjectData);
		gAgentAvatarp->sendAppearanceMessage(msg);
	}
	else
	{
		// If the textures are not baked, send NULL for texture IDs. This means
		// the baked texture IDs on the server will be untouched. Once all
		// textures are baked, another AvatarAppearance message will be sent to
		// update the TEs.
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addBinaryDataFast(_PREHASH_TextureEntry, NULL, 0);
	}

	for (LLViewerVisualParam* param =
			(LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam(); param;
		 param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
	{
		// Do not transmit params of group
		// VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT
		if (param->getGroup() == VISUAL_PARAM_GROUP_TWEAKABLE)
		{
			msg->nextBlockFast(_PREHASH_VisualParam);
			// We do not send the param ids. Instead, we assume that the
			// receiver has the same params in the same sequence.
			const F32 param_value = param->getWeight();
			const U8 new_weight = F32_to_U8(param_value, param->getMinWeight(),
											param->getMaxWeight());
			msg->addU8Fast(_PREHASH_ParamValue, new_weight);
		}
	}

	sendReliableMessage();
}

void LLAgent::sendAgentDataUpdateRequest()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessageFast(_PREHASH_AgentDataUpdateRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	sendReliableMessage();
}

//static
void LLAgent::userInfoRequestCallback(const LLSD& result, bool success)
{
	if (success && result.isMap() && result.has("success") &&
		result["success"].asBoolean())
	{
		// Note: support for setting the IM to email redirection with the
		// viewer has been removed from SL in November 2021... Kept only for
		// OpenSim. HB
		bool im_via_email = result.has("im_via_email") &&
							result["im_via_email"].asBoolean();
		bool verified = result["is_verified"].asBoolean();
		std::string email = result["email"].asString();
		std::string dir_vis = result["directory_visibility"].asString();
		LLFloaterPreference::updateUserInfo(dir_vis, im_via_email, email,
											verified ? 1 : 0);
		LLFloaterPostcard::updateUserInfo(email);
	}
	else
	{
		llwarns << "Failed to get user info via capability, falling back to UDP message."
				<< llendl;
		gAgent.sendAgentUserInfoRequestMessage();
	}
}

void LLAgent::sendAgentUserInfoRequest()
{
	if (gAgentID.notNull())
	{
		httpCallback_t succ = boost::bind(&LLAgent::userInfoRequestCallback,
										  _1, true);
		httpCallback_t fail = boost::bind(&LLAgent::userInfoRequestCallback,
										  _1, false);
		if (!gAgent.requestGetCapability("UserInfo", succ, fail))
		{
			sendAgentUserInfoRequestMessage();
		}
	}
}

void LLAgent::sendAgentUserInfoRequestMessage()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg && gAgentID.notNull())
	{
		msg->newMessageFast(_PREHASH_UserInfoRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		sendReliableMessage();
	}
}

//static
void LLAgent::userInfoUpdateCallback(const LLSD& result, bool success,
									 bool im_via_email, std::string dir_vis)
{
	if (!success || !result.isMap() || !result.has("success") ||
		!result["success"].asBoolean())
	{
		llwarns << "Failed to set user info via capability, falling back to UDP message."
				<< llendl;
		gAgent.sendAgentUserInfoRequestMessage(im_via_email, dir_vis);
	}
}

void LLAgent::sendAgentUpdateUserInfo(bool im_via_email,
									  const std::string& dir_visibility)
{
	if (gAgentID.isNull())
	{
		return;	// Not logged in ?
	}

	LLSD body;
	body["dir_visibility"] = LLSD::String(dir_visibility);
	// Note: support for setting the IM to email redirection with the viewer
	// has been removed from SL in November 2021... Kept only for OpenSim. HB
	if (!gIsInSecondLife)
	{
		body["im_via_email"] = LLSD::Boolean(im_via_email);
	}

	httpCallback_t succ = boost::bind(&LLAgent::userInfoUpdateCallback, _1,
									  true, im_via_email, dir_visibility);
	httpCallback_t fail = boost::bind(&LLAgent::userInfoUpdateCallback, _1,
									  false, im_via_email, dir_visibility);

	if (!gAgent.requestPostCapability("UserInfo", body, succ, fail))
	{
		sendAgentUserInfoRequestMessage(im_via_email, dir_visibility);
	}
}

// Note: support for setting the IM to email redirection with the viewer has
// been removed from SL in November 2021... Kept only for OpenSim. HB
void LLAgent::sendAgentUserInfoRequestMessage(bool im_via_email,
											  const std::string& dir_vis)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!gIsInSecondLife && msg && gAgentID.notNull())
	{
		msg->newMessageFast(_PREHASH_UpdateUserInfo);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_UserData);
		msg->addBoolFast(_PREHASH_IMViaEMail, im_via_email);
		msg->addString(_PREHASH_DirectoryVisibility,  dir_vis);
		sendReliableMessage();
	}
}

void LLAgent::observeFriends()
{
	if (!mFriendObserver)
	{
		mFriendObserver = new LLAgentFriendObserver;
		gAvatarTracker.addObserver(mFriendObserver);
		friendsChanged();
	}
}

void LLAgent::parseTeleportMessages(const std::string& xml_filename)
{
	LLXMLNodePtr root;
	bool success = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);
	if (!success || !root || !root->hasName("teleport_messages"))
	{
		llerrs << "Problem reading teleport string XML file: "
			   << xml_filename << llendl;
		return;
	}

	for (LLXMLNode* message_set = root->getFirstChild();
		 message_set != NULL;
		 message_set = message_set->getNextSibling())
	{
		if (!message_set->hasName("message_set")) continue;

		std::map<std::string, std::string>* teleport_msg_map = NULL;
		std::string message_set_name;

		if (message_set->getAttributeString("name", message_set_name))
		{
			// Now we loop over all the string in the set and add them to the
			// appropriate set
			if (message_set_name == "errors")
			{
				teleport_msg_map = &sTeleportErrorMessages;
			}
			else if (message_set_name == "progress")
			{
				teleport_msg_map = &sTeleportProgressMessages;
			}
		}

		if (!teleport_msg_map) continue;

		std::string message_name;
		for (LLXMLNode* message_node = message_set->getFirstChild();
			 message_node != NULL;
			 message_node = message_node->getNextSibling())
		{
			if (message_node->hasName("message") &&
				message_node->getAttributeString("name", message_name))
			{
				(*teleport_msg_map)[message_name] = message_node->getTextContents();
			}
		}
	}
}

//MK
//static
bool LLAgent::canWear(LLWearableType::EType type)
{
	return gRLenabled ? gRLInterface.canWear(type) : true;
}

//static
bool LLAgent::canUnwear(LLWearableType::EType type)
{
	return gRLenabled ? gRLInterface.canUnwear(type) : true;
}
//mk

/*****************************************************************************/
// Methods that used to be in llglsandbox.cpp

bool LLAgent::setLookAt(ELookAtType target_type, LLViewerObject* object,
						LLVector3 position)
{
	// No look at for far objects when PrivateLookAt is true
	static LLCachedControl<bool> private_look_at(gSavedSettings,
												 "PrivateLookAt");
	static LLCachedControl<U32> look_at_limit(gSavedSettings,
											  "PrivateLookAtLimit");
	if (private_look_at && object && target_type != LOOKAT_TARGET_NONE)
	{
		if ((object->getPositionGlobal() -
			 gAgent.getPositionGlobal()).length() > look_at_limit)
		{
			target_type = LOOKAT_TARGET_NONE;
			object = gAgentAvatarp;
			position.clear();
		}
	}

	if (object && object->isAttachment())
	{
		LLViewerObject* parentp = object;
		while (parentp)
		{
			if (parentp == gAgentAvatarp)
			{
				// looking at an attachment on ourselves, which we don't want
				// to do
				object = gAgentAvatarp;
				position.clear();
			}
			parentp = (LLViewerObject*)parentp->getParent();
		}
	}
	if (!mLookAt || mLookAt->isDead())
	{
		mLookAt =
			(LLHUDEffectLookAt*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_LOOKAT);
		mLookAt->setSourceObject(gAgentAvatarp);
	}

	return mLookAt->setLookAt(target_type, object, position);
}

bool LLAgent::setPointAt(EPointAtType target_type, LLViewerObject* object,
						 LLVector3 position)
{
	// Disallow pointing at attachments and avatars
	if (object && (object->isAttachment() || object->isAvatar()))
	{
		return false;
	}

	// No point at for far objects when PrivatePointAt is true
	static LLCachedControl<bool> private_point_at(gSavedSettings,
												  "PrivatePointAt");
	static LLCachedControl<U32> point_at_limit(gSavedSettings,
											   "PrivatePointAtLimit");
	if (private_point_at && object &&
		target_type != POINTAT_TARGET_NONE &&
		target_type != POINTAT_TARGET_CLEAR)
	{
		if ((object->getPositionGlobal() -
			 gAgent.getPositionGlobal()).length() > point_at_limit)
		{
			target_type = POINTAT_TARGET_NONE;
			object = NULL;
			position.clear();
		}
	}

	if (!mPointAt || mPointAt->isDead())
	{
		mPointAt = (LLHUDEffectPointAt*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_POINTAT);
		mPointAt->setSourceObject(gAgentAvatarp);
	}

	return mPointAt->setPointAt(target_type, object, position);
}

/*****************************************************************************/

LLAgentQueryManager gAgentQueryManager;

LLAgentQueryManager::LLAgentQueryManager()
:	mWearablesCacheQueryID(0),
	mNumPendingQueries(0),
	mUpdateSerialNum(0)
{
	for (U32 i = 0; i < BAKED_NUM_INDICES; ++i)
	{
		mActiveCacheQueries[i] = 0;
	}
}
