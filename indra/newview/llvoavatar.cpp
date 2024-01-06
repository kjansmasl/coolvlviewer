/**
 * @file llvoavatar.cpp
 * @brief Implementation of LLVOAvatar class which is a derivation of
 * LLViewerObject
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

#include <stdio.h>
#include <ctype.h>

#include "llvoavatar.h"

#include "imageids.h"
#include "llanimationstates.h"
#include "llapp.h"
#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "lleditingmotion.h"
#include "llevents.h"					// For gEventPumps
#include "llfasttimer.h"
#include "llheadrotmotion.h"
#include "llkeyframefallmotion.h"
#include "llkeyframestandmotion.h"
#include "llkeyframewalkmotion.h"
#include "llnoise.h"
#include "llnotifications.h"
#include "llphysshapebuilderutil.h"		// For COLLISION_TOLERANCE
#include "llpolyskeletaldistortion.h"
#include "llraytrace.h"
#include "llrenderutils.h"				// For gSphere
#include "llscriptpermissions.h"
#include "llsdserialize.h"
#include "lltargetingmotion.h"
#include "lltrans.h"
#include "sound_ids.h"

#include "llagent.h"					// Get state values from here
#include "llagentpilot.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llappviewer.h"				// For gFrameCount
#include "llavatartracker.h"			// For LLAvatarTracker::isAgentFriend()
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "llemote.h"
#include "llfirstuse.h"
#include "llgesturemgr.h"				// Needed to trigger the voice gestures
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llhudeffectspiral.h"
#include "llhudmanager.h"
#include "llhudtext.h"
#include "llinventorybridge.h"
#include "llmanipscale.h"
#include "llmeshrepository.h"
#include "hbobjectbackup.h"				// HBObjectBackup::validateAssetPerms()
#include "llphysicsmotion.h"
#include "llpipeline.h"
#include "llpuppetmodule.h"
#include "llpuppetmotion.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermedia.h"
#include "llviewermessage.h"			// For send_generic_message()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llviewertexlayer.h"
#include "llviewertexturelist.h"
#include "llviewerwearable.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llvoiceclient.h"
#include "llvoicevisualizer.h"
#include "llvovolume.h"
#include "llworld.h"

constexpr U32 JOINT_COUNT_REQUIRED_FOR_FULLRIG = 1;

constexpr F32 MAX_ATTACHMENT_COMPLEXITY = 1.0e6f;
constexpr F32 COMPLEXITY_UPDATE_INTERVAL = 10.f;

using namespace LLAvatarAppearanceDefines;

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// We clamp measured delta_time to this
constexpr F32 DELTA_TIME_MIN = 0.01f;
// Range to insure stability of computations
constexpr F32 DELTA_TIME_MAX = 0.2f;

// Pelvis follow half life while flying
constexpr F32 PELVIS_LAG_FLYING = 0.22f;
// Pelvis follow half life while walking
constexpr F32 PELVIS_LAG_WALKING = 0.4f;
constexpr F32 PELVIS_LAG_MOUSELOOK = 0.15f;
constexpr F32 MOUSELOOK_PELVIS_FOLLOW_FACTOR = 0.5f;

// Amount of deviation allowed between the pelvis and the view direction
// when moving fast & slow
constexpr F32 PELVIS_ROT_THRESHOLD_SLOW = 60.f;
constexpr F32 PELVIS_ROT_THRESHOLD_FAST = 2.f;
// Amount of deviation from up-axis, in degrees
constexpr F32 TORSO_NOISE_AMOUNT = 1.f;
// Time scale factor on torso noise
constexpr F32 TORSO_NOISE_SPEED = 0.2f;

constexpr F32 BREATHE_ROT_MOTION_STRENGTH = 0.05f;

constexpr S32 MIN_REQUIRED_PIXEL_AREA_BODY_NOISE = 10000;
constexpr S32 MIN_REQUIRED_PIXEL_AREA_BREATHE = 10000;
constexpr S32 MIN_REQUIRED_PIXEL_AREA_PELVIS_FIX = 40;

constexpr F32 HEAD_MOVEMENT_AVG_TIME = 0.9f;

constexpr S32 MORPH_MASK_REQUESTED_DISCARD = 0;

// Discard level at which to switch to baked textures. Should probably be 4 or
// 3, but did not want to change it while change other logic - SJB
constexpr S32 SWITCH_TO_BAKED_DISCARD = 5;

constexpr F32 HOVER_EFFECT_MAX_SPEED = 3.f;
constexpr F32 HOVER_EFFECT_STRENGTH = 0.f;
constexpr F32 UNDERWATER_EFFECT_STRENGTH = 0.1f;
constexpr F32 UNDERWATER_FREQUENCY_DAMP = 0.33f;
constexpr F32 APPEARANCE_MORPH_TIME = 0.65f;
constexpr F32 TIME_BEFORE_MESH_CLEANUP = 5.f; // seconds
// Number of avatar instances before releasing memory:
constexpr S32 AVATAR_RELEASE_THRESHOLD = 10;
constexpr F32 FOOT_GROUND_COLLISION_TOLERANCE = 0.25f;
constexpr F32 AVATAR_LOD_TWEAK_RANGE = 0.7f;
constexpr S32 MAX_BUBBLE_CHAT_LENGTH = DB_CHAT_MSG_STR_LEN;
constexpr S32 MAX_BUBBLE_CHAT_UTTERANCES = 12;
constexpr F32 CHAT_FADE_TIME = 8.0;
constexpr F32 BUBBLE_CHAT_TIME = CHAT_FADE_TIME * 3.f;

constexpr F32 DERUTHING_TIMEOUT_SECONDS = 60.f;

enum ERenderName
{
	RENDER_NAME_NEVER,
	RENDER_NAME_FADE,
	RENDER_NAME_ALWAYS
};

//-----------------------------------------------------------------------------
// Callback data
//-----------------------------------------------------------------------------

struct LLTextureMaskData
{
	LLTextureMaskData(const LLUUID& id)
	:	mAvatarID(id),
		mLastDiscardLevel(S32_MAX)
	{
	}

	LLUUID	mAvatarID;
	S32		mLastDiscardLevel;
};

/*********************************************************************************
 **                                                                             **
 ** Begin private LLVOAvatar Support classes
 **
 **/

//-----------------------------------------------------------------------------
// class LLBodyNoiseMotion
//-----------------------------------------------------------------------------
class LLBodyNoiseMotion final : public LLMotion
{
public:
	LLBodyNoiseMotion(const LLUUID& id)
	:	LLMotion(id)
	{
		mName = "body_noise";
		mTorsoState = new LLJointState;
	}

	// Functions to support MotionController and MotionRegistry

	// Static constructor. All subclasses must implement such a method and
	// register it.
	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLBodyNoiseMotion(id);
	}

	// Animation callbacks to be implemented by subclasses

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override				{ return true; }

	// Lotions must report their total duration
	LL_INLINE F32 getDuration() override			{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override		{ return 0.f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override		{ return 0.f; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override
	{
		return LLJoint::HIGH_PRIORITY;
	}

	LL_INLINE LLMotionBlendType getBlendType() override
	{
	return ADDITIVE_BLEND;
	}

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override
	{
		return MIN_REQUIRED_PIXEL_AREA_BODY_NOISE;
	}

	// Run-time (post constructor) initialization, called after parameters have
	// been set; must return true to indicate success and be available for
	// activation.
	LLMotionInitStatus onInitialize(LLCharacter* character) override
	{
		if (!mTorsoState->setJoint(character->getJoint(LL_JOINT_KEY_TORSO)))
		{
			return STATUS_FAILURE;
		}

		mTorsoState->setUsage(LLJointState::ROT);

		addJointState(mTorsoState);
		return STATUS_SUCCESS;
	}

	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated
	LL_INLINE bool onActivate() override			{ return true; }

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override
	{
		F32 nx[2];
		nx[0] = time * TORSO_NOISE_SPEED;
		nx[1] = 0.f;
		F32 ny[2];
		ny[0] = 0.f;
		ny[1] = time * TORSO_NOISE_SPEED;
		F32 noiseX = noise2(nx);
		F32 noiseY = noise2(ny);

		F32 rx = TORSO_NOISE_AMOUNT * DEG_TO_RAD * noiseX / 0.42f;
		F32 ry = TORSO_NOISE_AMOUNT * DEG_TO_RAD * noiseY / 0.42f;
		LLQuaternion tQn;
		tQn.setEulerAngles(rx, ry, 0.f);
		mTorsoState->setRotation(tQn);

		return true;
	}

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override			{}

private:
	// Joint states to be animated
	LLPointer<LLJointState> mTorsoState;
};

//-----------------------------------------------------------------------------
// class LLBreatheMotionRot
//-----------------------------------------------------------------------------
class LLBreatheMotionRot final : public LLMotion
{
public:
	LLBreatheMotionRot(const LLUUID& id)
	:	LLMotion(id),
		mBreatheRate(1.f),
		mCharacter(NULL)
	{
		mName = "breathe_rot";
		mChestState = new LLJointState;
	}

	// Methods to support MotionController and MotionRegistry

	// Static constructor
	// all subclasses must implement such a function and register it
	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLBreatheMotionRot(id);
	}

	// Animation callbacks to be implemented by subclasses

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override				{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override			{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override		{ return 0.f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override		{ return 0.f; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override
	{
		return LLJoint::MEDIUM_PRIORITY;
	}

	LL_INLINE LLMotionBlendType getBlendType() override
	{
		return NORMAL_BLEND;
	}

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override
	{
		return MIN_REQUIRED_PIXEL_AREA_BREATHE;
	}

	// Run-time (post constructor) initialization, called after parameters have
	// been set must return true to indicate success and be available for
	// activation
	LLMotionInitStatus onInitialize(LLCharacter* character) override
	{
		mCharacter = character;
		bool success = true;

		if (!mChestState->setJoint(character->getJoint(LL_JOINT_KEY_CHEST)))
		{
			success = false;
		}

		if (success)
		{
			mChestState->setUsage(LLJointState::ROT);
			addJointState(mChestState);
		}

		if (success)
		{
			return STATUS_SUCCESS;
		}
		else
		{
			return STATUS_FAILURE;
		}
	}

	// Called when a motion is activated, must return true to indicate success,
	// or else it will be deactivated.
	LL_INLINE bool onActivate() override			{ return true; }

	// Called per time step, must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override
	{
		mBreatheRate = 1.f;

		F32 breathe_amt = (sinf(mBreatheRate * time) * BREATHE_ROT_MOTION_STRENGTH);

		mChestState->setRotation(LLQuaternion(breathe_amt,
											  LLVector3(0.f, 1.f, 0.f)));

		return true;
	}

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override			{}

private:
	//-------------------------------------------------------------------------
	// joint states to be animated
	//-------------------------------------------------------------------------
	LLPointer<LLJointState>	mChestState;
	F32						mBreatheRate;
	LLCharacter*			mCharacter;
};

//-----------------------------------------------------------------------------
// class LLPelvisFixMotion
//-----------------------------------------------------------------------------
class LLPelvisFixMotion final : public LLMotion
{
public:
	LLPelvisFixMotion(const LLUUID& id)
	:	LLMotion(id), mCharacter(NULL)
	{
		mName = "pelvis_fix";
		mPelvisState = new LLJointState;
	}

	// functions to support MotionController and MotionRegistry

	// Static constructor
	// all subclasses must implement such a function and register it
	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLPelvisFixMotion(id);
	}

	// Animation callbacks to be implemented by subclasses

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override				{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override			{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override		{ return 0.5f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override		{ return 0.5f; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override
	{
		return LLJoint::LOW_PRIORITY;
	}

	LL_INLINE LLMotionBlendType getBlendType() override
	{
		return NORMAL_BLEND;
	}

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage.
	LL_INLINE F32 getMinPixelArea() override
	{
		return MIN_REQUIRED_PIXEL_AREA_PELVIS_FIX;
	}

	// Run-time (post constructor) initialization, called after parameters have
	// been set/ must return true to indicate success and be available for
	// activation
	LLMotionInitStatus onInitialize(LLCharacter* character) override
	{
		mCharacter = character;

		if (!mPelvisState->setJoint(character->getJoint(LL_JOINT_KEY_PELVIS)))
		{
			return STATUS_FAILURE;
		}

		mPelvisState->setUsage(LLJointState::POS);

		addJointState(mPelvisState);
		return STATUS_SUCCESS;
	}

	// Called when a motion is activated, must return true to indicate success,
	// or else it will be deactivated.
	LL_INLINE bool onActivate() override			{ return true; }

	// Called per time step, must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override
	{
		mPelvisState->setPosition(LLVector3::zero);

		return true;
	}

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override			{}

private:
	// joint states to be animated
	LLPointer<LLJointState>	mPelvisState;

	LLCharacter*			mCharacter;
};

/**
 **
 ** End LLVOAvatar Support classes
 **                                                                          **
 *****************************************************************************/

//-----------------------------------------------------------------------------
// Static Data
//-----------------------------------------------------------------------------
const LLUUID LLVOAvatar::sStepSoundOnLand = SND_STEP_ON_LAND;
const LLUUID LLVOAvatar::sStepSounds[LL_MCODE_END] =
{
	SND_STONE_RUBBER,
	SND_METAL_RUBBER,
	SND_GLASS_RUBBER,
	SND_WOOD_RUBBER,
	SND_FLESH_RUBBER,
	SND_RUBBER_PLASTIC,
	SND_RUBBER_RUBBER
};

LLAvatarAppearanceDictionary* LLVOAvatar::sAvatarDictionary = NULL;
std::string LLVOAvatar::sAgentAppearanceServiceURL;
F32 LLVOAvatar::sRenderDistance = 256.f;
F32 LLVOAvatar::sLODFactor = 1.f;
F32 LLVOAvatar::sPhysicsLODFactor = 1.f;
S32	LLVOAvatar::sNumVisibleAvatars = 0;
S32	LLVOAvatar::sNumLODChangesThisFrame = 0;
S32 LLVOAvatar::sRenderName = RENDER_NAME_ALWAYS;
S32 LLVOAvatar::sNumVisibleChatBubbles = 0;
U32 LLVOAvatar::sMaxNonImpostors = 50;
U32 LLVOAvatar::sMaxNonImpostorsPuppets = 0;
bool LLVOAvatar::sUseImpostors = false;
bool LLVOAvatar::sUsePuppetImpostors = false;
bool LLVOAvatar::sAvatarCullingDirty = false;
bool LLVOAvatar::sRenderGroupTitles = true;
bool LLVOAvatar::sDebugInvisible = false;
bool LLVOAvatar::sShowAttachmentPoints = false;
bool LLVOAvatar::sShowAnimationDebug = false;
bool LLVOAvatar::sVisibleInFirstPerson = false;
bool LLVOAvatar::sAvatarPhysics = false;
bool LLVOAvatar::sJointDebug = false;

F32 LLVOAvatar::sUnbakedTime = 0.f;
F32 LLVOAvatar::sUnbakedUpdateTime = 0.f;
F32 LLVOAvatar::sGreyTime = 0.f;
F32 LLVOAvatar::sGreyUpdateTime = 0.f;

LLVOAvatar::colors_map_t LLVOAvatar::sMinimapColorsMap;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
static F32 calc_bouncy_animation(F32 x);

LLVOAvatar::LLVOAvatar(const LLUUID& id, LLViewerRegion* regionp)
:	LLAvatarAppearance(&gAgentWearables),
	LLViewerObject(id, LL_PCODE_LEGACY_AVATAR, regionp),
	mSpecialRenderMode(0),
	mAttachmentSurfaceArea(0.f),
	mAttachmentGeometryBytes(0),
	mTurning(false),
	mLastSkeletonSerialNum(0),
	mIsSitting(false),
	mTimeVisible(),
	mTyping(false),
	mMeshValid(false),
	mVisible(false),
	mNeedsImpostorUpdate(true),
	mNeedsAnimUpdate(true),
	mNeedsExtentUpdate(false),
	mNextFrameForExtentUpdate(0),
	mDirtyMesh(2),				// Dirty geometry, need to regenerate.
	mMeshTexturesDirty(false),
	mSpeed(0.f),
	mSpeedAccum(0.f),
	mTimeLast(0.f),
	mRippleTimeLast(0.f),
	mWindFreq(0.f),
	mRipplePhase(0.f),
	mBelowWater(false),
	mInAir(false),
	mStepOnLand(true),
	mStepMaterial(0),
	mLastAppearanceBlendTime(0.f),
	mEnableDefaultMotions(true),
	mAppearanceAnimating(false),
	mWasOnGroundLeft(false),
	mWasOnGroundRight(false),
	mLipSyncActive(false),
	mOohMorph(NULL),
	mAahMorph(NULL),
	mCurrentGesticulationLevel(0),
	mNewResident(false),
	mNameAway(false),
	mNameBusy(false),
	mNameTyping(false),
	mNameMute(-1),
	mNameAppearance(false),
	mCachedVisualMuteUpdateTime(0.f),
	mCachedVisualMute(false),
//MK
	mCachedRLVMute(false),
//mk
	mRenderGroupTitles(sRenderGroupTitles),
	mFirstTEMessageReceived(false),
	mFirstAppearanceMessageReceived(false),
	mCulled(false),
	mVisibilityRank(0),
	mNeedsSkin(false),
	mLastSkinTime(0.f),
	mUpdatePeriod(1),
	mImpostorDistance(0.f),
	mImpostorPixelArea(0.f),
	mVisualComplexityStale(true),
	mComplexityUpdateTime(0.f),
	mVisualComplexity(0),
	mVisuallyMuteSetting(AV_RENDER_NORMALLY),
	mMutedAVColor(LLColor4::white),
	mFullyLoaded(false),
	mPreviousFullyLoaded(false),
	mFullyLoadedInitialized(false),
	mLoadedCallbacksPaused(false),
	mIsEditingAppearance(false),
	mUseLocalAppearance(false),
	mUseServerBakes(false),
	mLastUpdateRequestCOFVersion(LLViewerInventoryCategory::VERSION_UNKNOWN),
	mLastUpdateReceivedCOFVersion(LLViewerInventoryCategory::VERSION_UNKNOWN)
{
	LL_DEBUGS("Avatar") << "Constructor (0x" << this << ") id:" << mID
						<< LL_ENDL;

	mAttachedObjectsVector.reserve(MAX_AGENT_ATTACHMENTS);

	setHoverOffset(LLVector3::zero);

	// mVoiceVisualizer is created by the hud effects manager and uses the HUD
	// effects pipeline. NOTE: there is no need sending the effect to sim (thus
	// the false second parameter).
	mVoiceVisualizer =
		(LLVoiceVisualizer*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_VOICE_VISUALIZER,
													   false);

	mPelvisp = NULL;
	mHeadp = NULL;

	// Set up animation variables
	setAnimationData("Speed", &mSpeed);

	setNumTEs(TEX_NUM_INDICES);

	mCanSelect = true;

	mSignaledAnimations.clear();
	mPlayingAnimations.clear();

	// Register our mute list observer, and run it once so to update the
	// corresponding cached values.
	LLMuteList::addObserver(this);
	onChange();

	mRuthTimer.reset();

	mMinimapColor = getMinimapColor(id);

	static const LLColor4 tag_color(gColors.getColor4U("AvatarNameColor"));
	mNameTagColor = tag_color;

	LL_DEBUGS("Avatar") << "Constructor end" << LL_ENDL;
}

LLVOAvatar::~LLVOAvatar()
{
	LL_DEBUGS("Avatar") << "LLVOAvatar Destructor (0x" << this << ") id:"
						<< mID << LL_ENDL;

	LLMuteList::removeObserver(this);

	for (attachment_map_t::iterator it = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mAttachmentPoints.clear();

	mDead = true;

	mAnimationSources.clear();
	LLLoadedCallbackEntry::cleanUpCallbackList(&mCallbackTextureList);

	LL_DEBUGS("Avatar") << "LLVOAvatar Destructor end" << LL_ENDL;
}

void LLVOAvatar::markDead()
{
	deleteNameTag();
	// The UI gets destroyed when we quit and mVoiceVisualizer is dereferenced
	// as a result !
	if (!LLApp::isExiting())
	{
		mVoiceVisualizer->markDead();
	}
	LLLoadedCallbackEntry::cleanUpCallbackList(&mCallbackTextureList);
	LLViewerObject::markDead();
}

//virtual
bool LLVOAvatar::isValid() const
{
	// This should only be called on ourself.
	if (!isSelf())
	{
		llerrs << "Invalid condition isSelf() == false" << llendl;
	}
	return true;
}

bool LLVOAvatar::isFullyBaked()
{
	if (mIsDummy) return true;
	if (getNumTEs() == 0) return false;

	bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if ((i != BAKED_SKIRT || wearing_skirt) &&
			i != BAKED_LEFT_ARM && i != BAKED_LEFT_LEG && i != BAKED_AUX1 &&
			i != BAKED_AUX2 && i != BAKED_AUX3 &&
			!isTextureDefined(mBakedTextureDatas[i].mTextureIndex))
		{
			return false;
		}
	}
	return true;
}

void LLVOAvatar::deleteLayerSetCaches(bool clearAll)
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if (mBakedTextureDatas[i].mTexLayerSet)
		{
			// ! BACKWARDS COMPATIBILITY !
			// Can be removed after hair baking is mandatory on the grid
			if ((i != BAKED_HAIR || isSelf()) && !clearAll)
			{
				mBakedTextureDatas[i].mTexLayerSet->deleteCaches();
			}
		}
		if (mBakedTextureDatas[i].mMaskTexName)
		{
			LLImageGL::deleteTextures(1,
									  (GLuint*)&(mBakedTextureDatas[i].mMaskTexName));
			mBakedTextureDatas[i].mMaskTexName = 0;
		}
	}
	stop_glerror();
}

//static
void LLVOAvatar::dumpBakedStatus()
{
	LLVector3d camera_pos_global = gAgent.getCameraPositionGlobal();

	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* inst = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (!inst || inst->isDead()) continue;

		llinfos << "Avatar ";

		LLNameValue* firstname = inst->getNVPair("FirstName");
		LLNameValue* lastname = inst->getNVPair("LastName");

		if (firstname)
		{
			llcont << firstname->getString();
		}
		if (lastname)
		{
			llcont << " " << lastname->getString();
		}

		llcont << " " << inst->mID;

		if (inst->isDead())
		{
			llcont << " DEAD (" << inst->getNumRefs() << " refs)";
		}

		if (inst->isSelf())
		{
			llcont << " (self)";
		}

		F64 dist_to_camera = (inst->getPositionGlobal() -
							  camera_pos_global).length();
		llcont << " " << dist_to_camera << "m ";

		llcont << " " << inst->mPixelArea << " pixels";

		if (inst->isVisible())
		{
			llcont << " (visible)";
		}
		else
		{
			llcont << " (not visible)";
		}

		if (inst->isFullyBaked())
		{
			llcont << " Baked";
		}
		else
		{
			llcont << " Unbaked (";

			for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator
					iter = gAvatarAppDictp->getBakedTextures().begin(),
					end = gAvatarAppDictp->getBakedTextures().end();
				 iter != end; ++iter)
			{
				const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
					iter->second;
				const ETextureIndex index = baked_dict->mTextureIndex;
				if (inst->isTextureDefined(index)) continue;

				const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
					gAvatarAppDictp->getTexture(index);
				if (!t_dict) continue;

				llcont << " " << t_dict->mName;
			}

			llcont << ") " << inst->getUnbakedPixelAreaRank();
			if (inst->isCulled())
			{
				llcont << " culled";
			}
		}
		llcont << llendl;
	}
}

//static
void LLVOAvatar::restoreGL()
{
	if (!isAgentAvatarValid()) return;

	gAgentAvatarp->setCompositeUpdatesEnabled(true);
	for (U32 i = 0, count = gAgentAvatarp->mBakedTextureDatas.size();
		 i < count; ++i)
	{
		gAgentAvatarp->invalidateComposite(gAgentAvatarp->getTexLayerSet(i),
										   false);
	}
	gAgentAvatarp->updateMeshTextures();
}

//static
void LLVOAvatar::destroyGL()
{
	deleteCachedImages();

	resetImpostors();
}

//static
void LLVOAvatar::resetImpostors()
{
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatar = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatar && !avatar->isDead())
		{
			avatar->mImpostor.release();
			avatar->mNeedsImpostorUpdate = true;
		}
	}
}

//static
void LLVOAvatar::deleteCachedImages(bool clearAll)
{
	if (LLViewerTexLayerSet::sHasCaches)
	{
		LL_DEBUGS("Avatar") << "Deleting layer set caches" << LL_ENDL;
		for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
		{
			LLVOAvatar* inst = (LLVOAvatar*)LLCharacter::sInstances[i];
			if (inst)
			{
				inst->deleteLayerSetCaches(clearAll);
			}
		}
		LLViewerTexLayerSet::sHasCaches = false;
	}

	gTexLayerStaticImageList.deleteCachedImages();
}

//static
void LLVOAvatar::initClass()
{
	llinfos << "Initializing settings." << llendl;
	sAvatarPhysics = gSavedSettings.getBool("AvatarPhysics");
	updateSettings();
	llinfos << "Use avatar physics: " << (sAvatarPhysics ? "yes" : "no")
			<< " - Use impostors: " << (sUseImpostors ? "yes" : "no")
			<< " - Max non-impostors: " << sMaxNonImpostors << llendl;
	LLVOAvatarPuppet::sRegionChangedSlot =
		gAgent.addRegionChangedCB(boost::bind(&LLVOAvatarPuppet::onRegionChanged));
}

void LLVOAvatar::cleanupClass()
{
	LLVOAvatarPuppet::sRegionChangedSlot.disconnect();
}

//virtual
void LLVOAvatar::initInstance()
{
	// Register motions
	if (LLCharacter::sInstances.size() == 1)
	{
		registerMotion(ANIM_AGENT_BUSY,			LLNullMotion::create);
		registerMotion(ANIM_AGENT_CROUCH,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_CROUCHWALK,	LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_EXPRESS_AFRAID,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_ANGER,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_BORED,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_CRY,			LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_DISDAIN,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_EMBARRASSED,	LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_FROWN,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_KISS,			LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_LAUGH,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_OPEN_MOUTH,	LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_REPULSED,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_SAD,			LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_SHRUG,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_SMILE,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_SURPRISE,		LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_TONGUE_OUT,	LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_TOOTHSMILE,	LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_WINK,			LLEmote::create);
		registerMotion(ANIM_AGENT_EXPRESS_WORRY,		LLEmote::create);
		registerMotion(ANIM_AGENT_FEMALE_RUN_NEW,
					   LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_FEMALE_WALK,	LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_FEMALE_WALK_NEW,
					   LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_RUN,			LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_RUN_NEW,		LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_STAND,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_STAND_1,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_STAND_2,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_STAND_3,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_STAND_4,		LLKeyframeStandMotion::create);
		registerMotion(ANIM_AGENT_STANDUP,		LLKeyframeFallMotion::create);
		registerMotion(ANIM_AGENT_TURNLEFT,		LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_TURNRIGHT,	LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_WALK,			LLKeyframeWalkMotion::create);
		registerMotion(ANIM_AGENT_WALK_NEW,		LLKeyframeWalkMotion::create);

		// Motions without a start/stop bit
		registerMotion(ANIM_AGENT_BODY_NOISE,	LLBodyNoiseMotion::create);
		registerMotion(ANIM_AGENT_BREATHE_ROT,	LLBreatheMotionRot::create);
		registerMotion(ANIM_AGENT_PHYSICS_MOTION,
					   LLPhysicsMotionController::create);
		registerMotion(ANIM_AGENT_EDITING,		LLEditingMotion::create);
		registerMotion(ANIM_AGENT_EYE,			LLEyeMotion::create);
		registerMotion(ANIM_AGENT_FLY_ADJUST,	LLFlyAdjustMotion::create);
		registerMotion(ANIM_AGENT_HAND_MOTION,	LLHandMotion::create);
		registerMotion(ANIM_AGENT_HEAD_ROT,		LLHeadRotMotion::create);
		registerMotion(ANIM_AGENT_PELVIS_FIX,	LLPelvisFixMotion::create);
		registerMotion(ANIM_AGENT_SIT_FEMALE,	LLKeyframeMotion::create);
		registerMotion(ANIM_AGENT_TARGET,		LLTargetingMotion::create);
		registerMotion(ANIM_AGENT_WALK_ADJUST,	LLWalkAdjustMotion::create);
		registerMotion(ANIM_AGENT_PUPPET_MOTION,
					   LLPuppetMotion::create);
	}

	LLAvatarAppearance::initInstance();

	// Preload specific motions here
	createMotion(ANIM_AGENT_CUSTOMIZE);
	createMotion(ANIM_AGENT_CUSTOMIZE_DONE);
	createMotion(ANIM_AGENT_PUPPET_MOTION);

	mVoiceVisualizer->setVoiceEnabled(gVoiceClient.getVoiceEnabled(mID));
}

LLPuppetMotion* LLVOAvatar::getPuppetMotion()
{
	return (LLPuppetMotion*)findMotion(ANIM_AGENT_PUPPET_MOTION);
}

//virtual
LLAvatarJoint* LLVOAvatar::createAvatarJoint()
{
	return new LLViewerJoint();
}

//virtual
LLAvatarJointMesh* LLVOAvatar::createAvatarJointMesh()
{
	return new LLViewerJointMesh();
}

//virtual
LLTexLayerSet* LLVOAvatar::createTexLayerSet()
{
	return new LLViewerTexLayerSet(this);
}

const LLVector3 LLVOAvatar::getRenderPosition() const
{
	if (mDrawable.isNull() || mDrawable->getGeneration() < 0)
	{
		return getPositionAgent();
	}

	if (!isRoot())
	{
		LLDrawable* parentp = mDrawable->getParent();
		if (parentp)
		{
			return getPosition() * parentp->getRenderMatrix();
		}
		else
		{
			return mDrawable->getPositionAgent();
		}
	}

	LLVector3 pos = mDrawable->getPositionAgent();
	F32 fixup;
	if (hasPelvisFixup(fixup))
	{
		// Apply a pelvis fixup (as defined by the avatar's skin)
		pos[VZ] += fixup;
	}
	return pos;
}

void LLVOAvatar::updateDrawable(bool force_damped)
{
	clearChanged(SHIFTED);
}

void LLVOAvatar::onShift(const LLVector4a& shift_vector)
{
	const LLVector3& shift = reinterpret_cast<const LLVector3&>(shift_vector);
	mLastAnimExtents[0] += shift;
	mLastAnimExtents[1] += shift;
	mNeedsImpostorUpdate = true;
	mNeedsAnimUpdate = true;
}

void LLVOAvatar::updateSpatialExtents(LLVector4a& new_min, LLVector4a& new_max)
{
	if (mDrawable.isNull() || isDead()) return;

	if (mNeedsExtentUpdate)
	{
		calculateSpatialExtents(new_min, new_max);
		mLastAnimExtents[0].set(new_min.getF32ptr());
		mLastAnimExtents[1].set(new_max.getF32ptr());
		if (mPelvisp)
		{
			mLastAnimBasePos = mPelvisp->getWorldPosition();
		}
		mNeedsExtentUpdate = false;
	}
	else if (mPelvisp)
	{
		LLVector3 new_base_pos = mPelvisp->getWorldPosition();
		LLVector3 shift = new_base_pos - mLastAnimBasePos;
		mLastAnimExtents[0] += shift;
		mLastAnimExtents[1] += shift;
		mLastAnimBasePos = new_base_pos;
	}

	if (isImpostor() && !needsImpostorUpdate())
	{
		LLVector3 delta =
			getRenderPosition() -
			((LLVector3(mDrawable->getPositionGroup().getF32ptr()) -
			  mImpostorOffset));
		new_min.load3((mLastAnimExtents[0] + delta).mV);
		new_max.load3((mLastAnimExtents[1] + delta).mV);
	}
	else
	{
		new_min.load3(mLastAnimExtents[0].mV);
		new_max.load3(mLastAnimExtents[1].mV);
		LLVector4a pos_group;
		pos_group.setAdd(new_min, new_max);
		pos_group.mul(0.5f);
		mImpostorOffset = LLVector3(pos_group.getF32ptr()) -
									getRenderPosition();
		mDrawable->setPositionGroup(pos_group);
	}
}

void LLVOAvatar::calculateSpatialExtents(LLVector4a& new_min,
										 LLVector4a& new_max)
{
	static LLVector4a temp1, temp2, temp3;

	if (isDead()) return;

	// Load position
#if 0	// This causes massive issues for in-sim TPs, since the pelvis world
		// position is not updated in real time, unlike getRenderPosition(). HB
	LLVector3 pos = mPelvisp->getWorldPosition();
	if (pos.isExactlyZero())
	{
		pos = getRenderPosition();
	}
	new_min.load3(pos.mV);
#else
	new_min.load3(getRenderPosition().mV);
#endif
	new_max = new_min;

	// Pad bounding box for starting joint, plus polymesh if applicable.
	// Subsequent calcs should be accurate enough to not need padding.
	static const LLVector4a padding(0.25f);
	new_min.sub(padding);
	new_max.add(padding);

	static LLCachedControl<U32> avbbox_detail(gSavedSettings,
											  "AvatarBoundingBoxComplexity");
	U32 box_detail = avbbox_detail;
	if (isPuppetAvatar())
	{
		// Animated objects do not show an actual avatar but do need to include
		// their rigged meshes in their bounding box.
		box_detail = 3;
	}
	// Stretch bounding box by joint positions. No point doing this for puppet
	// avatars, where the polymeshes are not maintained or displayed.
	else if (box_detail >= 1)
	{
		for (polymesh_map_t::iterator i = mPolyMeshes.begin(),
									  end = mPolyMeshes.end();
			 i != end; ++i)
		{
			LLPolyMesh* mesh = i->second;
			if (!mesh) continue;	// Paranoia

			for (S32 joint_num = 0, count = mesh->mJointRenderData.size();
				 joint_num < count; ++joint_num)
			{
				// Load translation:
				temp1.load3(mesh->mJointRenderData[joint_num]->mWorldMatrix->getTranslation().mV);

				update_min_max(new_min, new_max, temp1);
			}
		}
	}

	// Stretch bounding box by static attachments
	if (box_detail >= 2)
	{
		// Max attachment span:
		temp1.splat(LLManipScale::maxPrimScale() * 5.f);

		for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
		{
			LLViewerObject* object = mAttachedObjectsVector[i].first;
			if (!object || object->isHUDAttachment())
			{
				continue;
			}

			LLVOVolume* vol = object->asVolume();
			if (vol && vol->isAnimatedObject())
			{
				// Animated objects already have a bounding box in their puppet
				// avatar, use that. They could lag by a frame if there is no
				// guarantee on order of processing for avatars.
				LLVOAvatarPuppet* puppet = vol->getPuppetAvatar();
				if (puppet)
				{
					temp2.load3(puppet->mLastAnimExtents[0].mV);
					temp3.load3(puppet->mLastAnimExtents[1].mV);
					update_min_max(new_min, new_max, temp2);
					update_min_max(new_min, new_max, temp3);
					continue;
				}
			}

			LLDrawable* drawable = object->mDrawable;
			if (!drawable || drawable->isState(LLDrawable::RIGGED |
											   // Do not extend box to children
											   LLDrawable::RIGGED_CHILD))
			{
				continue;
			}

			LLSpatialBridge* bridge = drawable->getSpatialBridge();
			if (bridge)
			{
				// Calculate distance:
				const LLVector4a* ext = bridge->getSpatialExtents();
				temp2.setSub(ext[1], ext[0]);

				// Only add the prim to spatial extents calculations if it is
				// not a megaprim (max attachment span calculated above is
				// currently 5 times our max prim size).
				S32 lt = temp2.lessThan(temp1).getGatheredBits() & 0x7;
				if (lt == 0x7)
				{
					update_min_max(new_min, new_max, ext[0]);
					update_min_max(new_min, new_max, ext[1]);
				}
			}
		}
	}

	// Stretch bounding box by rigged mesh joint boxes
	if (box_detail >= 3 && !isImpostor())
	{
		if (box_detail >= 4 || mJointRiggingInfoTab.needsUpdate())
		{
			updateRiggingInfo();
			mJointRiggingInfoTab.setNeedsUpdate(false);
		}
		static LLMatrix4a mat;
		static LLVector4a new_extents[2];
		for (U32 i = 0, count = mJointRiggingInfoTab.size(); i < count; ++i)
		{
			LLJointRiggingInfo* rig_info = &mJointRiggingInfoTab[i];
			if (!rig_info->isRiggedTo()) continue;

			// Note: joint key 0 = "unnamed", 1 = "mScreen" (so we skip them)
			LLJoint* jointp = getJoint(i + 2);
			if (!jointp) continue;

			mat.loadu(jointp->getWorldMatrix());
			mat.matMulBoundBox(rig_info->getRiggedExtents(), new_extents);
			update_min_max(new_min, new_max, new_extents[0]);
			update_min_max(new_min, new_max, new_extents[1]);
		}
	}

	// Update pixel area. First, calculate center.
	temp1.setAdd(new_min, new_max);
	temp1.mul(0.5f);
	// Calculate size.
	temp2.setSub(new_max, new_min);
	temp2.mul(0.5f);
	mPixelArea = LLPipeline::calcPixelArea(temp1, temp2, gViewerCamera);
}

void render_sphere_and_line(const LLVector3& begin_pos,
							const LLVector3& end_pos, F32 sphere_scale,
							const LLVector3& occ_color,
							const LLVector3& visible_color)
{
	// Unoccluded bone portions
	LLGLDepthTest normal_depth(GL_TRUE);

	// Draw line segment for unoccluded joint
	gGL.diffuseColor3f(visible_color[0], visible_color[1], visible_color[2]);

	gGL.begin(LLRender::LINES);
	gGL.vertex3fv(begin_pos.mV);
	gGL.vertex3fv(end_pos.mV);
	gGL.end();

	// Draw sphere representing joint pos
	gGL.pushMatrix();
	gGL.scalef(sphere_scale, sphere_scale, sphere_scale);
	gSphere.renderGGL();
	gGL.popMatrix();

	LLGLDepthTest depth_under(GL_TRUE, GL_FALSE, GL_GREATER);

	// Occluded bone portions
	gGL.diffuseColor3f(occ_color[0], occ_color[1], occ_color[2]);

	gGL.begin(LLRender::LINES);
	gGL.vertex3fv(begin_pos.mV);
	gGL.vertex3fv(end_pos.mV);
	gGL.end();

	// Draw sphere representing joint pos
	gGL.pushMatrix();
	gGL.scalef(sphere_scale, sphere_scale, sphere_scale);
	gSphere.renderGGL();
	gGL.popMatrix();
}

void LLVOAvatar::renderCollisionVolumes()
{
	constexpr F32 SPHERE_SCALE = 1.f;
	constexpr F32 CENTER_DOT_SCALE = 0.05f;
	static const LLVector3 CV_COLOR_OCCLUDED(0.f, 0.f, 1.f);
	static const LLVector3 CV_COLOR_OCC_PUPPET(0.f, 1.f, 1.f);
	static const LLVector3 CV_COLOR_VISIBLE(0.5f, 0.5f, 1.f);
	static const LLVector3 CV_COLOR_VIS_PUPPET(0.5f, 1.f, 1.f);
	static const LLVector3 DOT_COLOR_OCCLUDED(1.f, 1.f, 1.f);
	static const LLVector3 DOT_COLOR_VISIBLE(1.f, 1.f, 1.f);

	for (S32 i = 0, count = mCollisionVolumes.size(); i < count; ++i)
	{
		LLAvatarJointCollisionVolume& colvol = *mCollisionVolumes[i];

		colvol.updateWorldMatrix();

		gGL.pushMatrix();
		gGL.multMatrix(colvol.getXform()->getWorldMatrix().getF32ptr());

		LLVector3 end_pos(colvol.getEnd());
		if (isPuppetAvatar())
		{
			render_sphere_and_line(LLVector3::zero, end_pos, SPHERE_SCALE,
								   CV_COLOR_OCC_PUPPET, CV_COLOR_VIS_PUPPET);
		}
		else
		{
			render_sphere_and_line(LLVector3::zero, end_pos, SPHERE_SCALE,
								   CV_COLOR_OCCLUDED, CV_COLOR_VISIBLE);
		}
		render_sphere_and_line(LLVector3::zero, end_pos, CENTER_DOT_SCALE,
							   DOT_COLOR_OCCLUDED, DOT_COLOR_VISIBLE);

		gGL.popMatrix();
	}

	if (mNameText.notNull() && !mNameText->isDead())
	{
		LLVector4a unused;
		mNameText->lineSegmentIntersect(unused, unused, unused, true);
	}
}

void LLVOAvatar::renderBones(const std::string& selected_joint)
{
	if (isImpostor()) return;

	static const LLVector3 COLOR_VISIBLE(0.5f, 0.5f, 0.5f);
	// For selected joint
	static const LLVector3 SELECTED_COLOR_OCCLUDED(1.f, 1.f, 0.f);
	// For bones with position overrides defined
	static const LLVector3 OVERRIDE_COLOR_OCCLUDED(1.f, 0.f, 0.f);
	// For bones which are rigged to by at least one attachment
	static const LLVector3 RIGGED_COLOR_OCCLUDED(0.f, 1.f, 1.f);
	// For bones with puppetry data
	static const LLVector3 PUPPETRY_COLOR_OCCLUDED(0.f, 0.f, 1.f);
	// For bones not otherwise colored
	static const LLVector3 OTHER_COLOR_OCCLUDED(0.f, 1.f, 0.f);
	constexpr F32 SPHERE_SCALEF = 0.001f;

	LLPuppetModule* modulep = LLPuppetModule::getInstance();
	const LLVector3* occ_color;
	LLVector3 pos;
	LLUUID mesh_id;
	LLGLEnable blend(GL_BLEND);
	for (avatar_joint_list_t::iterator iter = mSkeleton.begin(),
									   end = mSkeleton.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (!jointp || !jointp->getXform())
		{
			continue;
		}

		jointp->updateWorldMatrix();

		F32 sphere_scale = SPHERE_SCALEF;
		if (jointp->getName() == selected_joint)
		{
			sphere_scale *= 16;
			occ_color = &SELECTED_COLOR_OCCLUDED;
		}
		else if (modulep->isActiveJoint(jointp->getName()))
		{
			occ_color = &PUPPETRY_COLOR_OCCLUDED;
		}
		else if (jointp->hasAttachmentPosOverride(pos, mesh_id))
		{
			occ_color = &OVERRIDE_COLOR_OCCLUDED;
		}
		else if (jointIsRiggedTo(jointp->getKey()))
		{
			occ_color = &RIGGED_COLOR_OCCLUDED;
		}
		else
		{
			occ_color = &OTHER_COLOR_OCCLUDED;
		}

		gGL.pushMatrix();
		gGL.multMatrix(jointp->getXform()->getWorldMatrix().getF32ptr());

		render_sphere_and_line(LLVector3::zero, jointp->getEnd(), sphere_scale,
							   *occ_color, COLOR_VISIBLE);

		gGL.popMatrix();
	}
}

void LLVOAvatar::renderJoints()
{
	if (isImpostor()) return;

	static const LLVector3 v[] =
	{
		LLVector3(0.1f, 0.f, 0.f),
		LLVector3(-0.1f, 0.f, 0.f),
		LLVector3(0.f, 0.1f, 0.f),
		LLVector3(0.f, -0.1f, 0.f),
		LLVector3(0.f, 0.f, -0.1f),
		LLVector3(0.f, 0.f, 0.1f),
	};

	for (joint_map_t::iterator iter = mJointMap.begin(), end = mJointMap.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = iter->second;
		if (!jointp || !jointp->getXform())
		{
			continue;
		}

		jointp->updateWorldMatrix();

		gGL.pushMatrix();
		gGL.multMatrix(jointp->getXform()->getWorldMatrix().getF32ptr());

		gGL.diffuseColor3f(1.f, 0.f, 1.f);

		gGL.begin(LLRender::LINES);

		// sides
		gGL.vertex3fv(v[0].mV);
		gGL.vertex3fv(v[2].mV);

		gGL.vertex3fv(v[0].mV);
		gGL.vertex3fv(v[3].mV);

		gGL.vertex3fv(v[1].mV);
		gGL.vertex3fv(v[2].mV);

		gGL.vertex3fv(v[1].mV);
		gGL.vertex3fv(v[3].mV);

		// top
		gGL.vertex3fv(v[0].mV);
		gGL.vertex3fv(v[4].mV);

		gGL.vertex3fv(v[1].mV);
		gGL.vertex3fv(v[4].mV);

		gGL.vertex3fv(v[2].mV);
		gGL.vertex3fv(v[4].mV);

		gGL.vertex3fv(v[3].mV);
		gGL.vertex3fv(v[4].mV);

		// bottom
		gGL.vertex3fv(v[0].mV);
		gGL.vertex3fv(v[5].mV);

		gGL.vertex3fv(v[1].mV);
		gGL.vertex3fv(v[5].mV);

		gGL.vertex3fv(v[2].mV);
		gGL.vertex3fv(v[5].mV);

		gGL.vertex3fv(v[3].mV);
		gGL.vertex3fv(v[5].mV);

		gGL.end();

		gGL.popMatrix();
	}
}

bool LLVOAvatar::lineSegmentIntersect(const LLVector4a& start,
									  const LLVector4a& end, S32 face,
									  bool pick_transparent, bool pick_rigged,
									  S32* face_hit, LLVector4a* intersection,
									  LLVector2* tex_coord, LLVector4a* normal,
									  LLVector4a* tangent)
{

	if (isPuppetAvatar() || (isSelf() && !gAgent.needsRenderAvatar()) ||
		!LLPipeline::sPickAvatar)
	{
		return false;
	}

	if (lineSegmentBoundingBox(start, end))
	{
		for (S32 i = 0, count = mCollisionVolumes.size(); i < count; ++i)
		{
			mCollisionVolumes[i]->updateWorldMatrix();

			LLMatrix4a mat(mCollisionVolumes[i]->getXform()->getWorldMatrix());

			LLMatrix4a inverse = mat;
			inverse.invert();

			LLMatrix4a norm_mat = inverse;
			norm_mat.transpose();

			LLVector4a p1, p2;
			// Might need to use perspectiveTransform here.
			inverse.affineTransform(start, p1);
			inverse.affineTransform(end, p2);

			LLVector3 position;
			LLVector3 norm;
			if (linesegment_sphere(LLVector3(p1.getF32ptr()),
								   LLVector3(p2.getF32ptr()),
								   LLVector3::zero, 1.f, position, norm))
			{
				if (intersection)
				{
					intersection->load3(position.mV);
					mat.affineTransform(*intersection, *intersection);
				}

				if (normal)
				{
					normal->load3(norm.mV);
					normal->normalize3fast();
					norm_mat.perspectiveTransform(*normal, *normal);
				}

				return true;
			}
		}

		if (isSelf())
		{
			for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count;
				 ++i)
			{
				LLViewerObject* object = mAttachedObjectsVector[i].first;
				if (object && !object->isDead() &&
					mAttachedObjectsVector[i].second->getValid())
				{
					LLDrawable* drawable = object->mDrawable;
					if (drawable && drawable->isState(LLDrawable::RIGGED))
					{
						// Regenerate octree for rigged attachment
						gPipeline.markRebuild(mDrawable,
											  LLDrawable::REBUILD_RIGGED);
					}
				}
			}
		}
	}

	LLVector4a position;
	if (mNameText.notNull() && !mNameText->isDead() &&
		mNameText->lineSegmentIntersect(start, end, position))
	{
		if (intersection)
		{
			*intersection = position;
		}

		return true;
	}

	return false;
}

//virtual
LLViewerObject* LLVOAvatar::lineSegmentIntersectRiggedAttachments(const LLVector4a& start,
																  const LLVector4a& end,
																  S32 face,
																  bool pick_transparent,
																  bool pick_rigged,
																  S32* face_hit,
																  LLVector4a* intersection,
																  LLVector2* tex_coord,
																  LLVector4a* normal,
																  LLVector4a* tangent)
{
	if (isSelf() && !gAgent.needsRenderAvatar())
	{
		return NULL;
	}

	LLViewerObject* hit = NULL;

	if (lineSegmentBoundingBox(start, end))
	{
		LLVector4a local_end = end;
		LLVector4a local_intersection;

		for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
		{
			LLViewerObject* object = mAttachedObjectsVector[i].first;
			if (object &&
				object->lineSegmentIntersect(start, local_end, face,
											 pick_transparent, pick_rigged,
											 face_hit, &local_intersection,
											 tex_coord, normal, tangent))
			{
				local_end = local_intersection;
				if (intersection)
				{
					*intersection = local_intersection;
				}
				hit = object;
			}
		}
	}

	return hit;
}

void LLVOAvatar::startDefaultMotions()
{
	if (mEnableDefaultMotions)
	{
		// Start default motions
		startMotion(ANIM_AGENT_HEAD_ROT);
		startMotion(ANIM_AGENT_EYE);
		startMotion(ANIM_AGENT_BODY_NOISE);
		startMotion(ANIM_AGENT_BREATHE_ROT);
		startMotion(ANIM_AGENT_PHYSICS_MOTION);
		startMotion(ANIM_AGENT_HAND_MOTION);
		startMotion(ANIM_AGENT_PELVIS_FIX);
	}
#if LL_ANIMESH_VPARAMS
	else
	{
		// Animated objects only support a subset
		startMotion(ANIM_AGENT_PHYSICS_MOTION);
	}
#endif

	// Restart any currently active motions
	processAnimationStateChanges();
}

// Deferred initialization and rebuild of the avatar.
//virtual
void LLVOAvatar::buildCharacter()
{
	LLAvatarAppearance::buildCharacter();

	// Not done building yet; more to do.
	mIsBuilt = false;

	// Set head offset from pelvis
	updateHeadOffset();

	// Initialize lip sync morph pointers

	mOohMorph = getVisualParam("Lipsync_Ooh");
	mAahMorph = getVisualParam("Lipsync_Aah");

	// If we do not have the Ooh morph, use the Kiss morph
	if (!mOohMorph)
	{
		llwarns << "Missing 'Ooh' morph for lipsync, using fallback."
				<< llendl;
		mOohMorph = getVisualParam("Express_Kiss");
	}

	// If we do not have the Aah morph, use the Open Mouth morph
	if (!mAahMorph)
	{
		llwarns << "Missing 'Aah' morph for lipsync, using fallback."
				<< llendl;
		mAahMorph = getVisualParam("Express_Open_Mouth");
	}

	if (mEnableDefaultMotions)
	{
		startDefaultMotions();
	}

	// Restart any currently active motions
	processAnimationStateChanges();

	mIsBuilt = true;
	mMeshValid = true;
}

void LLVOAvatar::resetVisualParams()
{
	// Skeletal params
	for (LLAvatarXmlInfo::skeletal_distortion_info_list_t::iterator
			iter = sAvatarXmlInfo->mSkeletalDistortionInfoList.begin(),
			end = sAvatarXmlInfo->mSkeletalDistortionInfoList.end();
		 iter != end; ++iter)
	{
		LLPolySkeletalDistortionInfo* info =
			(*iter)->asPolySkeletalDistortionInfo();
		if (!info) continue;

		S32 id = info->getID();
		LLPolySkeletalDistortion* param =
			getVisualParam(id)->asPolySkeletalDistortion();
		if (param)
		{
			*param = LLPolySkeletalDistortion(this);
			if (!param->setInfo(info))
			{
				llwarns << "Failed to set skeletal distortion for: " << id
						<< llendl;
			}
		}
		else
		{
			llwarns << "Failed to find skeletal distortion param for: " << id
					<< llendl;
		}
	}

	// Driver parameters
	for (LLAvatarXmlInfo::driver_info_list_t::iterator
			iter = sAvatarXmlInfo->mDriverInfoList.begin(),
			end = sAvatarXmlInfo->mDriverInfoList.end();
		 iter != end; ++iter)
	{
		LLDriverParamInfo* info = *iter;
		if (!info) continue;	// Paranoia

		LLVisualParam* vparam = getVisualParam(info->getID());
		if (!vparam) continue;

		LLDriverParam* param = vparam->asDriverParam();
		if (!param) continue;

		LLDriverParam::entry_list_t driven_list = param->getDrivenList();
		*param = LLDriverParam(this);
		if (param && param->setInfo(info))
		{
			param->setDrivenList(driven_list);
		}
	}
}

void LLVOAvatar::resetSkeleton()
{
	if (!mLastProcessedAppearance && !isPuppetAvatar())
	{
		llwarns << "No appearance message received yet: cannot reset avatar."
				<< llendl;
		return;
	}

	// Clear all attachment position and scale overrides
	clearAttachmentOverrides();

	// Reset the joints lookup cache
	mJointMap.clear();

	// Note that we call buildSkeleton twice in this function. The first time
	// is just to get the right scale for the collision volumes, because this
	// will be used in setting the mJointScales for the
	// LLPolySkeletalDistortions of which the collision volumes are children
	if (!buildSkeleton(sAvatarSkeletonInfo))
	{
		llwarns << "Could not rebuild " << getFullname(true) << "'s skeleton !"
				<< llendl;
	}

	// Reset some params to default state, without propagating changes
	// downstream
	resetVisualParams();

	// Now we have to reset the skeleton again, because its state got clobbered
	// by the resetVisualParams() calls above
	if (!buildSkeleton(sAvatarSkeletonInfo))
	{
		llwarns << "Could not rebuild " << getFullname(true) << "'s skeleton !"
				<< llendl;
	}

	// Reset attachment points (buildSkeleton only does bones and CVs)
	// but we still need to reinit HUDs (for self) since huds can be animated.
	initAttachmentPoints(!isSelf());	// true to ignore HUD joints

	// Fix up collision volumes
	for (LLVisualParam* param = getFirstVisualParam(); param;
		 param = getNextVisualParam())
	{
		LLPolyMorphTarget* pmorph = param->asPolyMorphTarget();
		if (pmorph)
		{
			// This is a kludgy way to correct for the fact that the collision
			// volumes have been reset out from under the poly morph sliders.
			F32 delta = pmorph->getLastWeight() - pmorph->getDefaultWeight();
			pmorph->applyVolumeChanges(delta);
		}
	}

	if (mLastProcessedAppearance)
	{
		// Reset/slam tweakable params to preserved state
		applyParsedAppearanceMessage(*mLastProcessedAppearance, true);
	}

	updateVisualParams();

	// Restore attachment pos overrides
	rebuildAttachmentOverrides();
}

void LLVOAvatar::releaseMeshData()
{
	if ((S32)sInstances.size() < AVATAR_RELEASE_THRESHOLD || isUIAvatar())
	{
		return;
	}

	// Cleanup mesh data
	for (avatar_joint_list_t::iterator iter = mMeshLOD.begin(),
									   end = mMeshLOD.end();
		 iter != end; ++iter)
	{
		LLAvatarJoint* joint = *iter;
		joint->setValid(false, true);
	}

	// Cleanup data
	if (mDrawable.notNull())
	{
		LLFace* facep = mDrawable->getFace(0);
		if (facep)
		{
			facep->setSize(0, 0);
			for (S32 i = mNumInitFaces, count = mDrawable->getNumFaces();
				 i < count; ++i)
			{
				facep = mDrawable->getFace(i);
				if (facep)
				{
					facep->setSize(0, 0);
				}
			}
		}
	}

	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment && !attachment->getIsHUDAttachment())
		{
			attachment->setAttachmentVisibility(false);
		}
	}

	mMeshValid = false;
}

//virtual
void LLVOAvatar::restoreMeshData()
{
	llassert(!isSelf());

	if (mDrawable.isNull()) return;

	mMeshValid = true;
	updateJointLODs();

	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment && !attachment->getIsHUDAttachment())
		{
			attachment->setAttachmentVisibility(true);
		}
	}

	// Force mesh update as LOD might not have changed to trigger this
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY);
}

void LLVOAvatar::updateMeshData()
{
	if (mDrawable.isNull())
	{
		return;
	}

	S32 f_num = 0;
	// Small number of this means each part of an avatar has its own vertex
	// buffer.
	constexpr U32 VERTEX_NUMBER_THRESHOLD = 128;
	const S32 num_parts = mMeshLOD.size();

	// This order is determined by number of LODS; if a mesh earlier in this
	// list changed LODs while a later mesh does not, the later mesh index
	// offset will be inaccurate
	for (S32 part_index = 0; part_index < num_parts; )
	{
		S32 j = part_index;
		U32 num_verts = 0;
		U32 num_indices = 0;
		U32 last_v_num = 0;
		U32 last_i_num = 0;

		while (part_index < num_parts && num_verts < VERTEX_NUMBER_THRESHOLD)
		{
			last_v_num = num_verts;
			last_i_num = num_indices;

			LLViewerJoint* part_mesh = getViewerJoint(part_index++);
			if (part_mesh)
			{
				part_mesh->updateFaceSizes(num_verts, num_indices,
										   mAdjustedPixelArea);
			}
		}
		if (num_verts < 1)
		{
			// Skip empty meshes
			continue;
		}
		if (last_v_num > 0)
		{
			// Put the last inserted part into next vertex buffer.
			num_verts = last_v_num;
			num_indices = last_i_num;
			--part_index;
		}

		LLFace* facep;
		if (f_num < mDrawable->getNumFaces())
		{
			facep = mDrawable->getFace(f_num);
		}
		else
		{
			facep = mDrawable->getFace(0);
			if (facep)
			{
				facep = mDrawable->addFace(facep->getPool(),
										   facep->getTexture());
			}
		}
		if (!facep)
		{
			continue;
		}

		// Resize immediately
		facep->setSize(num_verts, num_indices);

		bool terse_update = false;

		facep->setGeomIndex(0);
		facep->setIndicesIndex(0);

		LLVertexBuffer* buffp = facep->getVertexBuffer();
		if (buffp && buffp->getNumIndices() == num_indices &&
			buffp->getNumVerts() == num_verts)
		{
			terse_update = true;
		}
		else
		{
			buffp = new LLVertexBuffer(LLDrawPoolAvatar::VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
			buffp->setOwner("LLVOAvatar");
#endif
			if (!buffp->allocateBuffer(num_verts, num_indices))
			{
				llwarns << "Failure to allocate a vertex buffer with "
						<< num_verts << " vertices and " << num_indices
						<< " indices" << llendl;
				// Attempt to create a dummy triangle
				facep->setSize(1, 3);
				buffp->allocateBuffer(1, 3);
				if (!buffp)	continue;
				buffp->resetVertexData();
				buffp->resetIndexData();
			}
			facep->setVertexBuffer(buffp);
		}

		// *HACK: avatars have their own pool, so we are detecting the case of
		// more than one avatar in the pool (thus > 0 instead of >= 0)
		if (facep->getGeomIndex() > 0)
		{
			llwarns << getFullname(true) << " has non-zero geom index: "
					<< facep->getGeomIndex() << llendl;
			llassert(false);
			continue;
		}

		if (buffp->getNumIndices() == num_indices &&
			buffp->getNumVerts() == num_verts)
		{
			for (S32 k = j; k < part_index; ++k)
			{
				bool rigid = false;
				if (k == MESH_ID_EYEBALL_LEFT || k == MESH_ID_EYEBALL_RIGHT)
				{
					// Eyeballs cannot have terse updates since they are never
					// rendered with the hardware skinning shader
					rigid = true;
				}
				LLViewerJoint* mesh = getViewerJoint(k);
				if (mesh)
				{
					mesh->updateFaceData(facep, mAdjustedPixelArea,
										 k == MESH_ID_HAIR,
										 terse_update && !rigid);
				}
			}
		}

		buffp->unmapBuffer();

		if (!f_num)
		{
			f_num += mNumInitFaces;
		}
		else
		{
			++f_num;
		}
	}
}

U32 LLVOAvatar::processUpdateMessage(LLMessageSystem* msg,
									 void** user_data, U32 block_num,
									 EObjectUpdateType update_type,
									 LLDataPacker* dp)
{
	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(msg, user_data,
													  block_num, update_type,
													  dp);

	if ((retval & LLViewerObject::INVALID_UPDATE) && isSelf())
	{
		// Tell the sim to cancel this update
		gAgent.teleportViaLocation(gAgent.getPositionGlobal());
	}

	return retval;
}

LLViewerFetchedTexture* LLVOAvatar::getBakedTextureImage(U8 te,
														 const LLUUID& id)
{
	if (id.isNull() && LLViewerFetchedTexture::sDefaultImagep.notNull())
	{
		return LLViewerFetchedTexture::sDefaultImagep;
	}
	if (id == IMG_DEFAULT_AVATAR || id == IMG_DEFAULT || id == IMG_INVISIBLE)
	{
		// Should already exist, do not need to find it on sim or baked texture
		// host.
		LLViewerFetchedTexture* texp = gTextureList.findImage(id);
		if (texp)
		{
			return texp;
		}
	}

	const std::string url = getImageURL(te, id);
	if (url.empty())
	{
		LL_DEBUGS("Avatar") << getFullname(true) << "Getting texture " << id
							<< " from host." << LL_ENDL;
		LLHost host = getObjectHost();
		return LLViewerTextureManager::getFetchedTexture(id, FTT_HOST_BAKE,
														 true,
														 LLGLTexture::BOOST_NONE,
														 LLViewerTexture::LOD_TEXTURE,
														 0, 0, host);
	}

	LL_DEBUGS("Avatar") << getFullname(true) << " - URL for texture "
						<< id << ": " << url << LL_ENDL;
	return LLViewerTextureManager::getFetchedTextureFromUrl(url,
															FTT_SERVER_BAKE,
															true,
															LLGLTexture::BOOST_NONE,
															LLViewerTexture::LOD_TEXTURE,
															0, 0, id);
}

LLViewerTexture* LLVOAvatar::getBakedTexture(U8 te)
{
	if (te < 0 || te >= BAKED_NUM_INDICES)
	{
		return NULL;
	}

	if (!isEditingAppearance())
	{
		ETextureIndex i = mBakedTextureDatas[te].mTextureIndex;
		if (!isTextureDefined(i))
		{
			return NULL;
		}

		LLViewerTexture* baked_img = getImage(i, 0);
		if (!baked_img)
		{
			return NULL;
		}

		return LLViewerTextureManager::staticCast(baked_img, true);
	}

	LLViewerTexLayerSet* layerset = getTexLayerSet(te);
	if (!layerset)
	{
		return NULL;
	}
	layerset->createComposite();
	layerset->setUpdatesEnabled(true);
	return layerset->getViewerComposite();
}

//virtual
S32 LLVOAvatar::setTETexture(U8 te, const LLUUID& id)
{
	if (!isIndexBakedTexture((ETextureIndex)te))
	{
		// Sim still sends some UUIDs for non-baked slots sometimes: ignore.
		return LLViewerObject::setTETexture(te, LLUUID::null);
	}
	return setTETextureCore(te, getBakedTextureImage(te, id));
}

void LLVOAvatar::idleUpdate(F64 time)
{
	LL_FAST_TIMER(FTM_AVATAR_UPDATE);

	if (isDead())
	{
		llwarns << "Idle update on dead avatar" << llendl;
		return;
	}

	U32 type = isPuppetAvatar() ? LLPipeline::RENDER_TYPE_PUPPET
								: LLPipeline::RENDER_TYPE_AVATAR;
 	if (!gPipeline.hasRenderType(type))
	{
		return;
	}

	S32 current_frame = LLViewerOctreeEntryData::getCurrentFrame();
	if (!mNeedsExtentUpdate)
	{
		
		mNeedsExtentUpdate = current_frame >= mNextFrameForExtentUpdate ||
							 mLastAnimExtents[0].isExactlyZero() ||
							 mLastAnimExtents[1].isExactlyZero();
	}
	// Extent update should be happening max once every 4 frames (and even
	// less often for far impostors - HB).
	if (mNeedsExtentUpdate)
	{
		mNextFrameForExtentUpdate = current_frame + llmax(4, mUpdatePeriod);
	}

	checkTextureLoading();

	// Force immediate pixel area update on avatars using last frames data
	// (before drawable or camera updates)
	setPixelAreaAndAngle();

	// Force asynchronous drawable update
	if (mDrawable.notNull())
	{
		LL_FAST_TIMER(FTM_JOINT_UPDATE);

		if (mIsSitting && getParent())
		{
			LLViewerObject* root_object = (LLViewerObject*)getRoot();
			LLDrawable* drawablep = root_object->mDrawable;
			// If this object has not already been updated by another avatar...
			if (drawablep) // && !drawablep->isState(LLDrawable::EARLY_MOVE))
			{
				if (root_object->isSelected())
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
			gPipeline.updateMoveDampedAsync(mDrawable);
		}
	}

	// Set alpha flag depending on state
	if (isSelf())
	{
		LLViewerObject::idleUpdate(time);

		// trigger fidget anims
		if (isAnyAnimationSignaled(AGENT_STAND_ANIMS, NUM_AGENT_STAND_ANIMS))
		{
			gAgent.fidget();
		}
	}
	else
	{
		// Should override the idleUpdate stuff and leave out the angular
		// update part.
		LLQuaternion rotation = getRotation();
		LLViewerObject::idleUpdate(time);
		setRotation(rotation);
	}

	// attach objects that were waiting for a drawable
	lazyAttach();

	// Animate the character. Store off last frame's root position to be
	// consistent with camera position.
	LLVector3 root_pos_last = mRoot->getWorldPosition();
	bool detailed_update = updateCharacter();
	bool voice_enabled = gVoiceClient.getVoiceEnabled(mID) &&
						 gVoiceClient.inProximalChannel();

	idleUpdateVoiceVisualizer(voice_enabled);
	idleUpdateMisc(detailed_update);
	idleUpdateAppearanceAnimation();
	if (detailed_update)
	{
		if (voice_enabled)
		{
			idleUpdateLipSync();
		}
		idleUpdateLoadingEffect();
		idleUpdateBelowWater();	// wind effect uses this
		idleUpdateWindEffect();
	}
	idleUpdateNameTag(root_pos_last);
	idleUpdateRenderComplexity();
}

void LLVOAvatar::idleUpdateVoiceVisualizer(bool voice_enabled)
{
	bool is_self = isSelf();
	// Disable voice visualizer when in mouselook
	mVoiceVisualizer->setVoiceEnabled(voice_enabled &&
									  !(is_self && gAgent.cameraMouselook()));
	if (!voice_enabled)
	{
		return;
	}

	// Only do gesture triggering for your own avatar, and only when you are
	// in a proximal channel.
	if (is_self)
	{
		// The following takes the voice signal and uses that to trigger
		// gesticulations.
		S32 last_level = mCurrentGesticulationLevel;
		mCurrentGesticulationLevel =
			mVoiceVisualizer->getCurrentGesticulationLevel();

		// If "current gesticulation level" changes, we catch this, and trigger
		// the new gesture
		if (last_level != mCurrentGesticulationLevel &&
			mCurrentGesticulationLevel != VOICE_GESTICULATION_LEVEL_OFF)
		{
			if (mCurrentGesticulationLevel >= 0 &&
				mCurrentGesticulationLevel <= 2)
			{
				std::string gesture_name =
					llformat("/voicelevel%d", mCurrentGesticulationLevel + 1);
				gGestureManager.triggerAndReviseString(gesture_name);
			}
			else
			{
				llwarns << "CurrentGesticulationLevel can be only 0, 1, or 2"
						<< llendl;
			}
		}
	}

	// If the avatar is speaking, then the voice amplitude signal is passed to
	// the voice visualizer. Also, here we trigger voice visualizer start and
	// stop speaking, so it can animate the voice symbol.
	//
	// Notice the calls to "gAgent.clearAFK()". This resets the timer that
	// determines how long the avatar has been "away", so that the avatar
	// does not lapse into away-mode (and slump over) while the user is still
	// talking.
	if (gVoiceClient.getIsSpeaking(mID))
	{
		if (!mVoiceVisualizer->getCurrentlySpeaking())
		{
			mVoiceVisualizer->setStartSpeaking();
		}

		mVoiceVisualizer->setSpeakingAmplitude(gVoiceClient.getCurrentPower(mID));

		if (is_self)
		{
			gAgent.clearAFK();
		}
	}
	else if (mVoiceVisualizer->getCurrentlySpeaking())
	{
		mVoiceVisualizer->setStopSpeaking();

		if (mLipSyncActive)
		{
			if (mOohMorph)
			{
				mOohMorph->setWeight(mOohMorph->getMinWeight(), false);
			}
			if (mAahMorph)
			{
				mAahMorph->setWeight(mAahMorph->getMinWeight(), false);
			}

			mLipSyncActive = false;
			LLCharacter::updateVisualParams();
			dirtyMesh();
		}
	}

	// Here we get the approximate head position and set as sound source for
	// the voice symbol (the following version uses a tweak of "mHeadOffset"
	// which handles sitting vs. standing)
	if (mIsSitting)
	{
		LLVector3 headOffset = LLVector3(0.f, 0.f, mHeadOffset.mV[2]);
		mVoiceVisualizer->setVoiceSourceWorldPosition(mRoot->getWorldPosition() +
													  headOffset);
	}
	else
	{
		LLVector3 tagPos = mRoot->getWorldPosition();
		tagPos[VZ] -= mPelvisToFoot;
		tagPos[VZ] += (mBodySize[VZ] + 0.125f);
		mVoiceVisualizer->setVoiceSourceWorldPosition(tagPos);
	}
}

static void override_bbox(LLDrawable* drawablep, LLVector4a* extents)
{
	drawablep->setSpatialExtents(extents[0], extents[1]);
	drawablep->setPositionGroup(LLVector4a::getZero());
	drawablep->movePartition();
}

void LLVOAvatar::idleUpdateMisc(bool detailed_update)
{
	if (sJointDebug)
	{
		llinfos << getFullname(true) << ": joint touches: "
				<< LLJoint::sNumTouches << " updates: "
				<< LLJoint::sNumUpdates << llendl;
	}

	LLJoint::sNumUpdates = 0;
	LLJoint::sNumTouches = 0;

	bool visible = isVisible() || mNeedsAnimUpdate;
	bool impostor_not_needing_update = isImpostor() && !mNeedsImpostorUpdate;


	// Update attachments positions
	if (detailed_update && !impostor_not_needing_update)
	{
		LL_FAST_TIMER(FTM_ATTACHMENT_UPDATE);

		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		bool selected_attachment = selection->getObjectCount() &&
								   selection->isAttachment();

		U32 draw_order = 0;
		LLVector4a extents[2];
		LLDrawable* drawablep;
		for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
		{
			LLViewerObject* attach_objp = mAttachedObjectsVector[i].first;
			LLViewerJointAttachment* attachment =
				mAttachedObjectsVector[i].second;
			if (!attachment || !attachment->getValid() ||
				!attach_objp || attach_objp->isDead() ||
				!(drawablep = attach_objp->mDrawable))
			{
				continue;
			}

			LLSpatialBridge* bridgep = drawablep->getSpatialBridge();
			bool visible_attachment = visible ||
									  !(bridgep && bridgep->getRadius() < 2.f);
			if (!visible_attachment)
			{
				continue;
			}

			constexpr S32 rigged_flags = LLDrawable::RIGGED |
										 LLDrawable::RIGGED_CHILD;
			bool rigged_bridge = drawablep->isState(rigged_flags) &&
								 bridgep && !bridgep->isDead();
			// Override rigged attachments' octree spatial extents with this
			// avatar's bounding box
			if (rigged_bridge)
			{
				// Transform avatar bounding box into the coordinate frame of
				// the attachment
				bridgep->transformExtents(mDrawable->getSpatialExtents(),
										  extents);
				override_bbox(drawablep, extents);
				// The bridge could have died in override_bbox() so we need to
				// update the bridge.
				bridgep = drawablep->getSpatialBridge();
			}

			if (selected_attachment)
			{
				gPipeline.updateMoveNormalAsync(drawablep);
			}
			else
			{
				gPipeline.updateMoveDampedAsync(drawablep);
			}

			if (bridgep && !bridgep->isDead())
			{
				if (rigged_bridge)
				{
					// Specialized updateMoveNormalAsync()-like move just for
					// rigged attachment spatial bridge
					bridgep->setState(LLDrawable::MOVE_UNDAMPED);
					bridgep->updateMove();
					bridgep->setState(LLDrawable::EARLY_MOVE);
					// Set draw order of spatial group, if any.
					LLSpatialGroup* group = drawablep->getSpatialGroup();
					if (group)
					{
						group->mAvatarp = this;
						group->mRenderOrder = draw_order++;
					}
				}
				else
				{
					gPipeline.updateMoveNormalAsync(bridgep);
				}
			}

			attach_objp->updateText();
		}
	}

	mNeedsAnimUpdate = false;

	if (impostor_not_needing_update)
	{
		LLVector4a ext[2];
		F32 distance;
		LLVector3 angle;

		getImpostorValues(ext, angle, distance);

		for (U32 i = 0; i < 3 && !mNeedsImpostorUpdate; ++i)
		{
			F32 cur_angle = angle.mV[i];
			F32 old_angle = mImpostorAngle.mV[i];
			F32 angle_diff = fabsf(cur_angle - old_angle);

			if (angle_diff > F_PI / 512.f * distance * mUpdatePeriod)
			{
				mNeedsImpostorUpdate = mNeedsExtentUpdate = true;
			}
		}

		if (detailed_update && !mNeedsImpostorUpdate)
		{
			// Update impostor if view angle, distance, or bounding box change
			// significantly
			F32 dist_diff = fabsf(distance - mImpostorDistance);
			if (mImpostorDistance != 0.f &&
				dist_diff / mImpostorDistance > 0.1f)
			{
				mNeedsImpostorUpdate = mNeedsExtentUpdate = true;
			}
			else
			{
#if 0			// Do this only once per frame, in updateSpatialExtents()
				calculateSpatialExtents(ext[0], ext[1]);
#else
				ext[0].load3(mLastAnimExtents[0].mV);
				ext[1].load3(mLastAnimExtents[1].mV);
#endif
				LLVector4a diff;
				diff.setSub(ext[1], mImpostorExtents[1]);
				if (diff.getLength3().getF32() > 0.05f)
				{
					mNeedsImpostorUpdate = mNeedsExtentUpdate = true;
				}
				else
				{
					diff.setSub(ext[0], mImpostorExtents[0]);
					if (diff.getLength3().getF32() > 0.05f)
					{
						mNeedsImpostorUpdate = mNeedsExtentUpdate = true;
					}
				}
			}
		}
	}

	if (mDrawable.notNull())
	{
		mDrawable->movePartition();

		// Force a move if sitting on an active object
		LLViewerObject* parent = (LLViewerObject*)getParent();
		if (parent && parent->mDrawable && parent->mDrawable->isActive())
		{
			gPipeline.markMoved(mDrawable, true);
		}
	}
}

void LLVOAvatar::idleUpdateAppearanceAnimation()
{
	// Update morphing params
	if (!mAppearanceAnimating)
	{
		return;
	}

	ESex avatar_sex = getSex();
	F32 appearance_anim_time = mAppearanceMorphTimer.getElapsedTimeF32();
	if (appearance_anim_time >= APPEARANCE_MORPH_TIME)
	{
		mAppearanceAnimating = false;
		for (LLVisualParam* param = getFirstVisualParam(); param;
			 param = getNextVisualParam())
		{
			if (param->isTweakable())
			{
				param->stopAnimating(false);
			}
		}
		updateVisualParams();
		if (isSelf())
		{
			gAgent.sendAgentSetAppearance();
		}
	}
	else
	{
		F32 morph_amt = calcMorphAmount();
		LLVisualParam* param;

		if (!isSelf())
		{
			// Animate only top level params for non-self avatars
			for (param = getFirstVisualParam(); param;
				 param = getNextVisualParam())
			{
				if (param->isTweakable())
				{
					param->animate(morph_amt, false);
				}
			}
		}

		// Apply all params
		for (param = getFirstVisualParam(); param;
			 param = getNextVisualParam())
		{
			param->apply(avatar_sex);
		}

		mLastAppearanceBlendTime = appearance_anim_time;
	}
	dirtyMesh();
}

F32 LLVOAvatar::calcMorphAmount()
{
	F32 appearance_anim_time = mAppearanceMorphTimer.getElapsedTimeF32();
	F32 blend_frac = calc_bouncy_animation(appearance_anim_time /
										   APPEARANCE_MORPH_TIME);
	F32 last_blend_frac = calc_bouncy_animation(mLastAppearanceBlendTime /
												APPEARANCE_MORPH_TIME);

	F32 morph_amt;
	if (last_blend_frac == 1.f)
	{
		morph_amt = 1.f;
	}
	else
	{
		morph_amt = (blend_frac - last_blend_frac) / (1.f - last_blend_frac);
	}

	return morph_amt;
}

void LLVOAvatar::idleUpdateLipSync()
{
	// Use the Lipsync_Ooh and Lipsync_Aah morphs for lip sync
	if (gVoiceClient.lipSyncEnabled() && gVoiceClient.getIsSpeaking(mID))
	{
		F32 ooh_morph_amount = 0.f;
		F32 aah_morph_amount = 0.f;

		mVoiceVisualizer->lipSyncOohAah(ooh_morph_amount, aah_morph_amount);

		if (mOohMorph)
		{
			F32 ooh_weight = mOohMorph->getMinWeight() +
							 ooh_morph_amount *
							 (mOohMorph->getMaxWeight() -
							  mOohMorph->getMinWeight());

			mOohMorph->setWeight(ooh_weight, false);
		}

		if (mAahMorph)
		{
			F32 aah_weight = mAahMorph->getMinWeight() +
							 aah_morph_amount *
							 (mAahMorph->getMaxWeight() -
							  mAahMorph->getMinWeight());

			mAahMorph->setWeight(aah_weight, false);
		}

		mLipSyncActive = true;
		LLCharacter::updateVisualParams();
		dirtyMesh();
	}
}

void LLVOAvatar::idleUpdateLoadingEffect()
{
	// Update visibility when avatar is partially loaded
	if (updateIsFullyLoaded()) // changed ?
	{
		if (isFullyLoaded())
		{
			deleteParticleSource();
			updateLOD();
		}
		else if (!mIsDummy && !isTooComplex())
		{
			// Fancy particle cloud designed by Brent
			LLPartSysData particle_parameters;
			particle_parameters.mPartData.mMaxAge = 4.f;
			particle_parameters.mPartData.mStartScale.mV[VX] = 0.8f;
			particle_parameters.mPartData.mStartScale.mV[VX] = 0.8f;
			particle_parameters.mPartData.mStartScale.mV[VY] = 1.f;
			particle_parameters.mPartData.mEndScale.mV[VX] = 0.02f;
			particle_parameters.mPartData.mEndScale.mV[VY] = 0.02f;
			particle_parameters.mPartData.mStartColor =
				LLColor4(1.f, 1.f, 1.f, 0.5f);
			particle_parameters.mPartData.mEndColor =
				LLColor4(1.f, 1.f, 1.f, 0.f);
			particle_parameters.mPartData.mStartScale.mV[VX] = 0.8f;
			particle_parameters.mPartImageID =
				LLViewerTexture::sCloudImagep->getID();
			particle_parameters.mMaxAge = 0.f;
			particle_parameters.mPattern =
				LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE;
			particle_parameters.mInnerAngle = F_PI;
			particle_parameters.mOuterAngle = 0.f;
			particle_parameters.mBurstRate = 0.02f;
			particle_parameters.mBurstRadius = 0.f;
			particle_parameters.mBurstPartCount = 1;
			particle_parameters.mBurstSpeedMin = 0.1f;
			particle_parameters.mBurstSpeedMax = 1.f;
			particle_parameters.mPartData.mFlags =
				LLPartData::LL_PART_INTERP_COLOR_MASK |
				LLPartData::LL_PART_INTERP_SCALE_MASK |
				LLPartData::LL_PART_EMISSIVE_MASK |
				LLPartData::LL_PART_TARGET_POS_MASK;
			setParticleSource(particle_parameters, getID());
		}
	}
}

void LLVOAvatar::idleUpdateWindEffect()
{
	// Update wind effect
	if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR) >=
			LLDrawPoolAvatar::SHADER_LEVEL_CLOTH)
	{
		F32 hover_strength = 0.f;
		F32 time_delta = mRippleTimer.getElapsedTimeF32() - mRippleTimeLast;
		mRippleTimeLast = mRippleTimer.getElapsedTimeF32();
		LLVector3 velocity = getVelocity();
		F32 speed = velocity.length();
#if 0	// RN: velocity varies too much frame to frame for this to work
		lerp(mRippleAccel, (velocity - mLastVel) * time_delta,
			 LLCriticalDamp::getInterpolant(0.02f));
#else
		mRippleAccel.clear();
#endif
		mLastVel = velocity;
		LLVector4 wind;
		wind.set(getRegion()->mWind.getVelocityNoisy(getPositionAgent(), 4.f) -
				 velocity);

		if (mInAir)
		{
			hover_strength = HOVER_EFFECT_STRENGTH *
							 llmax(0.f, HOVER_EFFECT_MAX_SPEED - speed);
		}

		if (mBelowWater)
		{
			// *TODO: make cloth flow more gracefully when underwater
			hover_strength += UNDERWATER_EFFECT_STRENGTH;
		}

		wind.mV[VZ] += hover_strength;
		wind.normalize();

		wind.mV[VW] = llmin(0.025f + speed * 0.015f + hover_strength, 0.5f);
		F32 interp;
		if (wind.mV[VW] > mWindVec.mV[VW])
		{
			interp = LLCriticalDamp::getInterpolant(0.2f);
		}
		else
		{
			interp = LLCriticalDamp::getInterpolant(0.4f);
		}
		mWindVec = lerp(mWindVec, wind, interp);

		F32 wind_freq = hover_strength +
						llclamp(8.f + speed * 0.7f +
								noise1(mRipplePhase) * 4.f,
								8.f, 25.f);
		mWindFreq = lerp(mWindFreq, wind_freq, interp);

		if (mBelowWater)
		{
			mWindFreq *= UNDERWATER_FREQUENCY_DAMP;
		}

		mRipplePhase += time_delta * mWindFreq;
		if (mRipplePhase > F_TWO_PI)
		{
			mRipplePhase = fmodf(mRipplePhase, F_TWO_PI);
		}
	}
}

void LLVOAvatar::idleUpdateNameTag(const LLVector3& root_pos_last)
{
	// Update chat bubble (draw text label over character's head)
	if (mChatTimer.getElapsedTimeF32() > BUBBLE_CHAT_TIME)
	{
		mChats.clear();
	}

	static LLCachedControl<F32> name_show_time(gSavedSettings,
											   "RenderNameShowTime");
	static LLCachedControl<F32> fade_duration(gSavedSettings,
											  "RenderNameFadeDuration");
	static LLCachedControl<bool> show_typing_info(gSavedSettings,
												  "ShowTypingInfo");
	static LLCachedControl<bool> use_chat_bubbles(gSavedSettings,
												  "UseChatBubbles");
	bool show_typing = show_typing_info && !use_chat_bubbles;
	bool visible_avatar = isVisible() || mNeedsAnimUpdate;
	bool visible_chat = use_chat_bubbles && (mChats.size() || mTyping);
	if (mTyping && show_typing && !visible_chat &&
		sRenderName == RENDER_NAME_FADE)
	{
		mTimeVisible.reset();
	}
	const F32 time_visible = mTimeVisible.getElapsedTimeF32();

	bool render_name =	visible_chat ||
						(visible_avatar &&
						 (sRenderName == RENDER_NAME_ALWAYS ||
						  (sRenderName == RENDER_NAME_FADE &&
						   time_visible < name_show_time)));
	// If it is our own avatar, do not draw in mouselook, and do not draw if we
	// are specifically hiding our own name.
	if (render_name && isSelf())
	{
		static LLCachedControl<bool> render_name_hide_self(gSavedSettings,
														   "RenderNameHideSelf");
		render_name = !gAgent.cameraMouselook() &&
					  (visible_chat || !render_name_hide_self);
	}
//MK
	// Hide the names above the heads if we are under @shownametags or
	// @shownames or if we are under @camdistdrawmin and the other avatar is
	// farther than the specified distance hide the names above the heads
	if (gRLenabled)
	{
		if (gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags)
		{
			render_name = false;
		}
		else if (gRLInterface.mCamDistDrawMin < EXTREMUM &&
				 isAgentAvatarValid() && gAgentAvatarp != this)
		{
			LLVector3 head_pos = gAgentAvatarp->mHeadp->getWorldPosition();
			LLVector3 camera_offset = mHeadp->getWorldPosition() - head_pos;
			F32 camera_distance = (F32)camera_offset.length();
			if (camera_distance > gRLInterface.mCamDistDrawMin)
			{
				render_name = false;
			}
		}
	}
//mk
	if (!render_name)
	{
		deleteNameTag();
		return;
	}

	bool new_name = false;
	if (visible_chat != mVisibleChat)
	{
		mVisibleChat = visible_chat;
		new_name = true;
	}

	if (sRenderGroupTitles != mRenderGroupTitles)
	{
		mRenderGroupTitles = sRenderGroupTitles;
		new_name = true;
	}

	// First Calculate alpha. If > 0, create mNameText if necessary, otherwise
	// delete it
	F32 alpha = 0.f;
	if (mAppAngle > 5.f)
	{
		const F32 start_fade_time = name_show_time - fade_duration;
		if (!visible_chat && sRenderName == RENDER_NAME_FADE &&
			time_visible > start_fade_time)
		{
			alpha = 1.f - (time_visible - start_fade_time) / fade_duration;
		}
		else
		{
			// ...not fading, full alpha
			alpha = 1.f;
		}
	}
	else if (mAppAngle > 2.f)
	{
		// Far away is faded out also
		alpha = (mAppAngle - 2.f) / 3.f;
	}
	if (alpha <= 0.f)
	{
		deleteNameTag();
		return;
	}

	if (mNameText.isNull() || mNameText->isDead())
	{
		mNameText =
			(LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
		mNameText->setMass(10.f);
		mNameText->setSourceObject(this);
		mNameText->setVertAlignment(LLHUDText::ALIGN_VERT_TOP);
		mNameText->setVisibleOffScreen(true);
		mNameText->setMaxLines(11);
		mNameText->setFadeDistance(CHAT_NORMAL_RADIUS, 5.f);
		mNameText->setUseBubble(true);
		++sNumVisibleChatBubbles;
		new_name = true;
	}

	LLColor4 name_color = mNameTagColor;
	name_color.setAlpha(alpha);
	mNameText->setColor(name_color);

	LLQuaternion root_rot = mRoot->getWorldRotation();
	mNameText->setUsePixelSize(true);
	LLVector3 pixel_right_vec;
	LLVector3 pixel_up_vec;
	gViewerCamera.getPixelVectors(root_pos_last, pixel_up_vec,
								  pixel_right_vec);
	LLVector3 camera_to_av = root_pos_last - gViewerCamera.getOrigin();
	camera_to_av.normalize();
	LLVector3 local_camera_at = camera_to_av * ~root_rot;
	LLVector3 local_camera_up = camera_to_av % gViewerCamera.getLeftAxis();
	local_camera_up.normalize();
	local_camera_up = local_camera_up * ~root_rot;

	local_camera_up.scaleVec(mBodySize * 0.5f);
	local_camera_at.scaleVec(mBodySize * 0.5f);

	LLVector3 name_position = mRoot->getWorldPosition();
	name_position[VZ] -= mPelvisToFoot;
	name_position[VZ] += (mBodySize[VZ] * 0.55f);
	name_position += local_camera_up * root_rot -
					 projected_vec(local_camera_at * root_rot,
								   camera_to_av);
	name_position += pixel_up_vec * 15.f;
	mNameText->setPositionAgent(name_position);

	LLNameValue* title = getNVPair("Title");
	LLNameValue* firstname = getNVPair("FirstName");
	LLNameValue* lastname = getNVPair("LastName");

	if (mNameText.notNull() && !mNameText->isDead() && firstname && lastname)
	{
		std::string complete_name = firstname->getString();
		std::string last = lastname->getString();
		if (!LLAvatarName::sOmitResidentAsLastName || last != "Resident")
		{
			if (sRenderGroupTitles)
			{
				complete_name += " ";
			}
			else
			{
				// If all group titles are turned off, stack first name on a
				// line above last name
				complete_name += "\n";
			}
			complete_name += last;
		}

		if (LLAvatarNameCache::useDisplayNames())
		{
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(mID, &avatar_name))
			{
				if (LLAvatarNameCache::useDisplayNames() == 2)
				{
					complete_name = avatar_name.mDisplayName;
				}
				else
				{
					complete_name = avatar_name.getNames(true);
				}
			}
		}

		anim_it_t end_signaled_anims = mSignaledAnimations.end();
		bool is_away =
			mSignaledAnimations.find(ANIM_AGENT_AWAY) != end_signaled_anims;
		bool is_busy =
			mSignaledAnimations.find(ANIM_AGENT_BUSY) != end_signaled_anims;
		bool is_appearance =
			mSignaledAnimations.find(ANIM_AGENT_CUSTOMIZE) != end_signaled_anims;
		bool chat_muted = mCachedMuteFlags == 0 ||
						 (mCachedMuteFlags != -1 &&
						  (mCachedMuteFlags & LLMute::flagTextChat));

		if (mNameString.empty() || new_name ||
			complete_name != mCompleteName ||
			(!title && !mTitle.empty()) ||
			(title && mTitle != title->getString()) ||
			is_away != mNameAway || is_busy != mNameBusy ||
			mCachedMuteFlags != mNameMute || is_appearance != mNameAppearance ||
			(show_typing && !chat_muted && mTyping != mNameTyping))
		{
			std::string line;
			if (sRenderGroupTitles && title && title->getString() &&
				title->getString()[0] != '\0')
			{
				line += title->getString();
				LLStringFn::replace_ascii_controlchars(line, LL_UNKNOWN_CHAR);
				line += "\n";
				line += complete_name;
			}
			else
			{
				line = complete_name;
			}

			bool need_comma = false;
			if (is_away || is_busy || mCachedMuteFlags != -1 ||
				(show_typing && mTyping))
			{
				line += "\n(";
				if (is_away)
				{
					line += "Away";
					need_comma = true;
				}
				if (is_busy)
				{
					if (need_comma)
					{
						line += ", ";
					}
					line += "Busy";
					need_comma = true;
				}
				if (show_typing && mTyping && !chat_muted)
				{
					if (need_comma)
					{
						line += ", ";
					}
					line += "Typing";
					need_comma = true;
				}
				if (mCachedMuteFlags != -1)
				{
					if (need_comma)
					{
						line += ", ";
					}
					line += mCachedMuteDesc;
				}
				line += ")";
			}
			if (is_appearance)
			{
				line += "\n";
				line += "(Editing Appearance)";
			}
			mNameAway = is_away;
			mNameBusy = is_busy;
			mNameTyping = mTyping;
			mNameMute = mCachedMuteFlags;
			mNameAppearance = is_appearance;
			mTitle = title ? title->getString() : "";
			mCompleteName = complete_name;
			mNameString = utf8str_to_wstring(line);
			new_name = true;
		}

		if (visible_chat)
		{
			mNameText->setDropShadow(true);
			mNameText->setFont(LLFontGL::getFontSansSerif());
			mNameText->setTextAlignment(LLHUDText::ALIGN_TEXT_LEFT);
			mNameText->setFadeDistance(CHAT_NORMAL_RADIUS * 2.f, 5.f);
			if (new_name)
			{
				mNameText->setLabel(mNameString);
			}

			std::deque<LLChat>::iterator chat_iter = mChats.begin();
			std::deque<LLChat>::iterator chat_end = mChats.end();
			mNameText->clearString();

			LLColor4 new_chat = mNameTagColor;
			LLColor4 normal_chat = lerp(new_chat,
										LLColor4(0.8f, 0.8f, 0.8f, 1.f),
										0.7f);
			LLColor4 old_chat = lerp(normal_chat,
									 LLColor4(0.6f, 0.6f, 0.6f, 1.f),
									 0.7f);
			if (mTyping && (S32)mChats.size() >= MAX_BUBBLE_CHAT_UTTERANCES)
			{
				++chat_iter;
			}

			for ( ; chat_iter != chat_end; ++chat_iter)
			{
				F32 chat_fade_amt =
					llclamp((F32)((LLFrameTimer::getElapsedSeconds() -
								   chat_iter->mTime) / CHAT_FADE_TIME),
							0.f, 4.f);
				LLFontGL::StyleFlags style;
				switch (chat_iter->mChatType)
				{
					case CHAT_TYPE_WHISPER:
						style = LLFontGL::ITALIC;
						break;

					case CHAT_TYPE_SHOUT:
						style = LLFontGL::BOLD;
						break;

					default:
						style = LLFontGL::NORMAL;
				}
				if (chat_fade_amt < 1.f)
				{
					F32 u = clamp_rescale(chat_fade_amt, 0.9f, 1.f, 0.f, 1.f);
					mNameText->addLine(utf8str_to_wstring(chat_iter->mText),
									   lerp(new_chat, normal_chat, u),
									   style);
				}
				else if (chat_fade_amt < 2.f)
				{
					F32 u = clamp_rescale(chat_fade_amt, 1.9f, 2.f, 0.f, 1.f);
					mNameText->addLine(utf8str_to_wstring(chat_iter->mText),
									   lerp(normal_chat, old_chat, u), style);
				}
				else if (chat_fade_amt < 3.f)
				{
					// *NOTE: only remove lines down to minimum number
					mNameText->addLine(utf8str_to_wstring(chat_iter->mText),
									   old_chat, style);
				}
			}
			mNameText->setVisibleOffScreen(true);

			if (mTyping)
			{
				S32 dot_count =
					(llfloor(mTypingTimer.getElapsedTimeF32() * 3.f) + 2) % 3 + 1;
				switch (dot_count)
				{
					case 1:
						mNameText->addLine(".", new_chat);
						break;

					case 2:
						mNameText->addLine("..", new_chat);
						break;

					case 3:
						mNameText->addLine("...", new_chat);
				}
			}
		}
		else
		{
			static LLCachedControl<bool> small_avatar_names(gSavedSettings,
															"SmallAvatarNames");
			if (small_avatar_names)
			{
				mNameText->setFont(LLFontGL::getFontSansSerif());
			}
			else
			{
				mNameText->setFont(LLFontGL::getFontSansSerifBig());
			}
			mNameText->setTextAlignment(LLHUDText::ALIGN_TEXT_CENTER);
			mNameText->setFadeDistance(CHAT_NORMAL_RADIUS, 5.f);
			mNameText->setVisibleOffScreen(false);
			if (new_name)
			{
				mNameText->setLabel("");
				mNameText->setString(mNameString);
			}
		}
	}
}

void LLVOAvatar::setMinimapColor(const LLColor4& color)
{
	mMinimapColor = color;
	static LLCachedControl<LLColor4U> map_avatar(gColors, "MapAvatar");
	static LLCachedControl<LLColor4U> map_friend(gColors, "MapFriend");
	bool is_friend = LLAvatarTracker::isAgentFriend(mID);
	LLColor4 expected_color = LLColor4(is_friend ? map_friend : map_avatar);
	if (expected_color != color)
	{
		sMinimapColorsMap.emplace(mID, color);
	}
	else
	{
		sMinimapColorsMap.erase(mID);
	}
}

//static
const LLColor4& LLVOAvatar::getMinimapColor(const LLUUID& id)
{
	if (!sMinimapColorsMap.empty())
	{
		colors_map_t::const_iterator it = sMinimapColorsMap.find(id);
		if (it != sMinimapColorsMap.end())
		{
			return it->second;
		}
	}

	static const LLColor4 normal_color(gColors.getColor4U("MapAvatar"));
	static const LLColor4 friend_color(gColors.getColor4U("MapFriend"));
	return LLAvatarTracker::isAgentFriend(id) ? friend_color : normal_color;
}

void LLVOAvatar::setNameTagColor(const LLColor4& color)
{
	mNameTagColor = color;
	if (mNameText.notNull() && !mNameText->isDead())
	{
		mNameText->setColor(color);
	}
}

void LLVOAvatar::deleteNameTag()
{
	if (mNameText.notNull() && !mNameText->isDead())
	{
		mNameText->markDead();
		mNameText = NULL;
		--sNumVisibleChatBubbles;
	}
}

void LLVOAvatar::clearNameTag()
{
	if (!mIsDummy)
	{
		mNameString.clear();
		if (mNameText.notNull() && !mNameText->isDead())
		{
			mNameText->setLabel("");
			mNameText->setString(mNameString);
		}
	}
}

//static
void LLVOAvatar::invalidateNameTag(const LLUUID& agent_id)
{
	LLVOAvatar* avatarp = gObjectList.findAvatar(agent_id);
	if (avatarp)
	{
		avatarp->clearNameTag();
	}
}

//static
void LLVOAvatar::invalidateNameTags()
{
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatar = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatar && !avatar->isDead())
		{
			avatar->clearNameTag();
		}
	}
}

void LLVOAvatar::idleUpdateBelowWater()
{
	if (getRegion())	// May be NULL on disconnect during TP
	{
		F32 avatar_height = (F32)(getPositionGlobal().mdV[VZ]);
		mBelowWater =  avatar_height < getRegion()->getWaterHeight();
	}
}

void LLVOAvatar::slamPosition()
{
	gAgent.setPositionAgent(getPositionAgent());
	mRoot->setWorldPosition(getPositionAgent()); // teleport
	setChanged(TRANSLATED);
	if (mDrawable.notNull())
	{
		gPipeline.updateMoveNormalAsync(mDrawable);
	}
	mRoot->updateWorldMatrixChildren();
}

//virtual
void LLVOAvatar::onChange()
{
	bool old_mute = mCachedMute;
	mCachedMute = LLMuteList::isMuted(mID);
	mCachedMuteFlags = LLMuteList::getMuteFlags(mID, mCachedMuteDesc);
	if (mCachedMute != old_mute)
	{
		mCachedVisualMuteUpdateTime = 0.f;
	}
	if (mCachedMute)
	{
		mVisuallyMuteSetting = AV_RENDER_NORMALLY;
	}
}

//virtual
bool LLVOAvatar::isVisuallyMuted()
{
	if (isDead()) return false;

	bool muted = mCachedVisualMute;

	if (gFrameTimeSeconds > mCachedVisualMuteUpdateTime)
	{
		mCachedVisualMuteUpdateTime = gFrameTimeSeconds + 5.f;
		mMutedAVColor = LLColor4::white;
		if (mVisuallyMuteSetting == AV_ALWAYS_RENDER)
		{
			muted = false;
		}
		else if (mVisuallyMuteSetting == AV_DO_NOT_RENDER)
		{
			muted = true;
			mMutedAVColor = LLColor4::grey3;
		}
		else if (mCachedMute)
		{
			muted = true;
			mMutedAVColor = LLColor4::grey4;
		}
		else
		{
			muted = isTooComplex();
			if (muted)
			{
				static LLCachedControl<bool> colored(gSavedSettings,
													 "ColoredJellyDolls");
				if (colored)
				{
					// Same silly calculations as in LL's viewer, just slightly
					// optimized...
					static LLColor4* spectrum_color[] = { &LLColor4::red,
														  &LLColor4::magenta,
														  &LLColor4::blue,
														  &LLColor4::cyan,
														  &LLColor4::green,
														  &LLColor4::yellow,
														  &LLColor4::red };
					constexpr F32 SCALING_FACTOR = 6.f / 256.f;
					F32 spectrum = F32(getID().mData[0]) * SCALING_FACTOR;
					S32 spectrum_index_1 = floor(spectrum);
					S32 spectrum_index_2 = spectrum_index_1 + 1;
					F32 fraction = spectrum - (F32)(spectrum_index_1);
					mMutedAVColor = lerp(*spectrum_color[spectrum_index_1],
										 *spectrum_color[spectrum_index_2],
										 fraction);
					mMutedAVColor.normalize();
					mMutedAVColor *= 0.28f;
				}
				else
				{
					mMutedAVColor = LLColor4::grey3;
				}
				LLFirstUse::useJellyDoll();
			}
		}
//MK
		bool old_rlv_mute = mCachedRLVMute;
		mCachedRLVMute = gRLenabled &&
						 gRLInterface.avatarVisibility(this) != 1;
		if (mCachedRLVMute)
		{
			if (old_rlv_mute != mCachedRLVMute)
			{
				LL_DEBUGS("RestrainedLove") << getFullname(true)
											<< " rendering is "
											<< (mCachedRLVMute ? "no more"
															   : "now")
											<< " restricted." << LL_ENDL;
			}
			mMutedAVColor = LLColor4::grey5;
			if (mVisuallyMuteSetting == AV_ALWAYS_RENDER)
			{
				mVisuallyMuteSetting = AV_RENDER_NORMALLY;
			}
			mCachedVisualMuteUpdateTime = gFrameTimeSeconds + 1.f;
		}
//mk
	}

	// We cannot visually mute without impostors !
	muted = muted && sUseImpostors;
	mCachedVisualMute = muted;

//MK
	muted |= mCachedRLVMute;
//mk
	return muted;
}

void LLVOAvatar::updateFootstepSounds()
{
	// Find the ground under each foot, these are used for a variety of things
	// that follow
	LLVector3 ankle_left_pos_agent = mFootLeftp->getWorldPosition();
	LLVector3 ankle_right_pos_agent = mFootRightp->getWorldPosition();

	LLVector3 ankle_left_ground_agent = ankle_left_pos_agent;
	LLVector3 ankle_right_ground_agent = ankle_right_pos_agent;
	LLVector3 normal;
	resolveHeightAgent(ankle_left_pos_agent, ankle_left_ground_agent, normal);
	resolveHeightAgent(ankle_right_pos_agent, ankle_right_ground_agent,
					   normal);

	F32 left_elev = llmax(-0.2f,
						  ankle_left_pos_agent.mV[VZ] -
						  ankle_left_ground_agent.mV[VZ]);
	F32 right_elev = llmax(-0.2f,
						   ankle_right_pos_agent.mV[VZ] -
						   ankle_right_ground_agent.mV[VZ]);
	if (!mIsSitting)
	{
		// Figure out which foot is on ground
		if (!mInAir && (left_elev < 0.f || right_elev < 0.f))
		{
			ankle_left_pos_agent = mFootLeftp->getWorldPosition();
			ankle_right_pos_agent = mFootRightp->getWorldPosition();
			left_elev = ankle_left_pos_agent.mV[VZ] -
					   ankle_left_ground_agent.mV[VZ];
			right_elev = ankle_right_pos_agent.mV[VZ] -
						ankle_right_ground_agent.mV[VZ];
		}
	}

	static const LLUUID AGENT_FOOTSTEP_ANIMS[] =
	{
		ANIM_AGENT_WALK,
		ANIM_AGENT_RUN,
		ANIM_AGENT_LAND
	};
	constexpr S32 NUM_AGENT_FOOTSTEP_ANIMS =
		LL_ARRAY_SIZE(AGENT_FOOTSTEP_ANIMS);

	if (gAudiop &&
		isAnyAnimationSignaled(AGENT_FOOTSTEP_ANIMS, NUM_AGENT_FOOTSTEP_ANIMS))
	{
		bool play_sound = false;
		LLVector3 foot_pos_agent;

		bool on_ground_left = left_elev <= 0.05f;
		bool on_ground_right = right_elev <= 0.05f;

		// Did left foot hit the ground ?
		if (on_ground_left && !mWasOnGroundLeft)
		{
			foot_pos_agent = ankle_left_pos_agent;
			play_sound = true;
		}

		// Did right foot hit the ground ?
		if (on_ground_right && !mWasOnGroundRight)
		{
			foot_pos_agent = ankle_right_pos_agent;
			play_sound = true;
		}

		mWasOnGroundLeft = on_ground_left;
		mWasOnGroundRight = on_ground_right;

		if (play_sound)
		{
#if 0
			F32 gain = clamp_rescale(mSpeedAccum,
									 AUDIO_STEP_LO_SPEED, AUDIO_STEP_HI_SPEED,
									 AUDIO_STEP_LO_GAIN, AUDIO_STEP_HI_GAIN);
#endif
			LLVector3d foot_pos = gAgent.getPosGlobalFromAgent(foot_pos_agent);

			if (gViewerParcelMgr.canHearSound(foot_pos) &&
				(mCachedMuteFlags & LLMute::flagObjectSounds))
			{
				constexpr F32 STEP_VOLUME = 0.5f;
				const LLUUID& step_sound_id = getStepSound();
				gAudiop->triggerSound(step_sound_id, mID, STEP_VOLUME,
									  LLAudioEngine::AUDIO_TYPE_AMBIENT,
									  foot_pos);
			}
		}
	}
}

void LLVOAvatar::computeUpdatePeriod(bool& visible)
{
	if (isSelf() || isUIAvatar())
	{
		// Never change the update period (always 1) for self and UI avatars
		return;
	}

	bool visually_muted = isVisuallyMuted();
	if (visible && mDrawable.notNull() && useImpostors() &&
		!mNeedsAnimUpdate)
	{
		const LLVector4a* ext = mDrawable->getSpatialExtents();
		LLVector4a size;
		size.setSub(ext[1], ext[0]);
		F32 mag = size.getLength3().getF32() * 0.5f;

		F32 impostor_area = 256.f * 512.f *
							(8.125f - LLVOAvatar::sLODFactor * 8.f);
		if (visually_muted
//MK
			&& !mCachedRLVMute)
//mk
		{
			// Muted avatars update REALLY slow
			mUpdatePeriod = 16;
		}
		else if (mVisibilityRank <= getMaxNonImpostors() ||
				 mDrawable->mDistanceWRTCamera < 1.f + mag)
		{
			// Max visible avatars are not impostored. Also, do not impostor
			// avatars whose bounding box may be penetrating the impostor
			// camera near clip plane
			mUpdatePeriod = 1;
			visible = true;
			return;
		}
		else if (mVisibilityRank > getMaxNonImpostors() * 4)
		{
			// Background avatars are REALLY slow updating impostors
			mUpdatePeriod = 16;
		}
		else if (mVisibilityRank > getMaxNonImpostors() * 3)
		{
			// Back 25% of max visible avatars are slow updating impostors
			mUpdatePeriod = 8;
		}
		else if (mImpostorPixelArea <= impostor_area)
		{
			// Stuff in between gets an update period based on pixel area
			mUpdatePeriod = llclamp((S32)sqrtf(impostor_area * 4.f /
											   mImpostorPixelArea), 2, 8);
		}
		else
		{
			// Nearby avatars, update the impostors more frequently.
			mUpdatePeriod = 4;
		}

		visible = (LLViewerOctreeEntryData::getCurrentFrame() +
				   mID.mData[0]) % mUpdatePeriod == 0;
	}
	else
	{
		mUpdatePeriod = 1;
	}
}

void LLVOAvatar::updateTimeStep()
{
	if (!isSelf() && !isUIAvatar())
	{
		F32 time_quantum = clamp_rescale((F32)sInstances.size(),
										 10.f, 35.f, 0.f, 0.25f);
		F32 pixel_area_scale = clamp_rescale(mPixelArea, 100, 5000, 1.f, 0.f);
		F32 time_step = time_quantum * pixel_area_scale;
		if (time_step != 0.f)
		{
			// Disable walk motion servo controller as it does not work with
			// motion timesteps
			stopMotion(ANIM_AGENT_WALK_ADJUST);
			removeAnimationData("Walk Speed");
		}
		mMotionController.setTimeStep(time_step);
		// We must take into account the slow down caused by any lowered update
		// rate.
		mMotionController.setTimeFactor((F32)mUpdatePeriod);
	}
}

void LLVOAvatar::updateRootPositionAndRotation(F32 speed, bool sat_on_ground)
{
	// This case includes all configurations except sitting on an object, so it
	// does include ground sit.
	if (!mIsSitting || !getParent())
	{
		// Get timing info. Handle initial condition case
		F32 animation_time = mAnimTimer.getElapsedTimeF32();
		if (mTimeLast <= 0.f)
		{
			mTimeLast = animation_time;

			// Put the pelvis at slaved position/mRotation
			mRoot->setWorldPosition(getPositionAgent()); // first frame
			mRoot->setWorldRotation(getRotation());
		}

		// Do not let dT get larger than 1/5th of a second
		F32 delta_time = animation_time - mTimeLast;

		delta_time = llclamp(delta_time, DELTA_TIME_MIN, DELTA_TIME_MAX);
		mTimeLast = animation_time;

		mSpeedAccum = mSpeedAccum * 0.95f + speed * 0.05f;

		// Compute the position of the avatar's root

		bool is_self = isSelf();
		if (is_self)
		{
			gAgent.setPositionAgent(getRenderPosition());
		}

		LLVector3d root_pos =
			gAgent.getPosGlobalFromAgent(getRenderPosition());
#if 1	// LL's viewer counts twice the Z offset
		static LLCachedControl<F32> factor(gSavedSettings,
										   "HoverToZOffsetFactor");
		if (factor > 1.f)
		{
			// Hover should not be accounted here, because it already is in the
			// avatar render position as sent by the server !!! HB
			root_pos.mdV[VZ] += getVisualParamWeight(AVATAR_HOVER);
		}
#endif
		LLVector3 normal;
		LLVector3d ground_under_pelvis;
		resolveHeightGlobal(root_pos, ground_under_pelvis, normal);
		F32 foot_to_ground = (F32)(root_pos.mdV[VZ] - mPelvisToFoot -
								   ground_under_pelvis.mdV[VZ]);

		bool in_air = !gWorld.getRegionFromPosGlobal(ground_under_pelvis) ||
					   foot_to_ground > FOOT_GROUND_COLLISION_TOLERANCE;
		if (in_air && !mInAir)
		{
			mTimeInAir.reset();
		}
		mInAir = in_air;

#if 0	// SL-427: this appears to be too frequent, moving to only do on
		// animation state change.
		computeBodySize();
#endif

		// Correct for the fact that the pelvis is not necessarily the center
		// of the agent's physical representation
		root_pos.mdV[VZ] -= 0.5f * mBodySize.mV[VZ] - mPelvisToFoot;
		if (!mIsSitting && !sat_on_ground)
		{
			root_pos += LLVector3d(getHoverOffset());
		}

		if (isPuppetAvatar())
		{
			((LLVOAvatarPuppet*)this)->matchVolumeTransform();
			return;
		}
		// Only for non-puppet av below this point

		LLVector3 new_pos = gAgent.getPosAgentFromGlobal(root_pos);
		if (new_pos != mRoot->getXform()->getWorldPosition())
		{
			mRoot->touch();
			mRoot->setWorldPosition(new_pos); // regular update
		}

		// Propagate viewer object rotation to root of avatar
		if (!isAnyAnimationSignaled(AGENT_NO_ROTATE_ANIMS,
									NUM_AGENT_NO_ROTATE_ANIMS))
		{
			// Compute a forward direction vector derived from the primitive
			// rotation and the velocity vector. When walking or jumping, do
			// not let the body deviate more than 90 from the view; if
			// necessary, flip the velocity vector.

			LLVector3 prim_dir;
			if (is_self)
			{
				prim_dir = gAgent.getAtAxis() -
						  projected_vec(gAgent.getAtAxis(),
										gAgent.getReferenceUpVector());
				prim_dir.normalize();
			}
			else
			{
				prim_dir = getRotation().getMatrix3().getFwdRow();
			}

			LLVector3 vel_dir = getVelocity();
			vel_dir.normalize();
			if (mSignaledAnimations.find(ANIM_AGENT_WALK) !=
					mSignaledAnimations.end())
			{
				F32 vpD = vel_dir * prim_dir;
				if (vpD < -0.5f)
				{
					vel_dir *= -1.f;
				}
			}
			LLVector3 fwd_dir = lerp(prim_dir, vel_dir,
									 clamp_rescale(speed, 0.5f, 2.f, 0.f,
												   1.f));
			if (isSelf() && gAgent.cameraMouselook())
			{
				// Make sure fwd_dir stays in same general direction as primdir
				if (gAgent.getFlying())
				{
					fwd_dir = gViewerCamera.getAtAxis();
				}
				else
				{
					LLVector3 at_axis = gViewerCamera.getAtAxis();
					LLVector3 up_vector = gAgent.getReferenceUpVector();
					at_axis -= up_vector * (at_axis * up_vector);
					at_axis.normalize();

					F32 dot = fwd_dir * at_axis;
					if (dot < 0.f)
					{
						fwd_dir -= 2.f * at_axis * dot;
						fwd_dir.normalize();
					}
				}
			}

			LLQuaternion root_rotation = mRoot->getWorldMatrix().quaternion();
			F32 root_roll, root_pitch, root_yaw;
			root_rotation.getEulerAngles(&root_roll, &root_pitch, &root_yaw);

			// When moving very slow, the pelvis is allowed to deviate from the
			// forward direction to allow it to hold its position while the
			// torso and head turn. Once in motion, it must conform however.
			bool self_in_mouselook = is_self && gAgent.cameraMouselook();

			F32 pelvis_rot_thres_slow = PELVIS_ROT_THRESHOLD_SLOW;
			if (is_self)
			{
				static LLCachedControl<bool> use_in_mouse_look(gSavedSettings,
															   "MouseLookUseRotDeviation");
				static LLCachedControl<U32> max_rot_deviation(gSavedSettings,
															  "CameraToPelvisRotDeviation");
				if (use_in_mouse_look || !self_in_mouselook)
				{
					pelvis_rot_thres_slow = llclamp((F32)max_rot_deviation,
													PELVIS_ROT_THRESHOLD_FAST,
													PELVIS_ROT_THRESHOLD_SLOW);
				}
			}
			LLVector3 pelvis_dir(mRoot->getWorldMatrix().getFwdRow4().mV);
			F32 pelvis_rot_thres = clamp_rescale(speed, 0.1f, 1.f,
												 pelvis_rot_thres_slow,
												 PELVIS_ROT_THRESHOLD_FAST);

			if (self_in_mouselook)
			{
				pelvis_rot_thres *= MOUSELOOK_PELVIS_FOLLOW_FACTOR;
			}
			pelvis_rot_thres *= DEG_TO_RAD;

			F32 angle = angle_between(pelvis_dir, fwd_dir);

			// The avatar's root is allowed to have a yaw that deviates widely
			// from the forward direction, but if roll or pitch are off even
			// a little bit we need to correct the rotation.
			if (root_roll < 1.f * DEG_TO_RAD && root_pitch < 5.f * DEG_TO_RAD)
			{
				// Smaller correction vector means pelvis follows prim direction
				// more closely
				if (!mTurning && angle > pelvis_rot_thres * 0.75f)
				{
					mTurning = true;
				}

				// Use tighter threshold when turning
				if (mTurning)
				{
					pelvis_rot_thres *= 0.4f;
				}

				// Am I done turning ?
				if (angle < pelvis_rot_thres)
				{
					mTurning = false;
				}

				LLVector3 correction_vector = (pelvis_dir - fwd_dir) *
											  clamp_rescale(angle,
															pelvis_rot_thres *
														    0.75f,
															pelvis_rot_thres,
															1.f, 0.f);
				fwd_dir += correction_vector;
			}
			else
			{
				mTurning = false;
			}

			// Now compute the full world space rotation for the whole body (wQv)
			LLVector3 up_dir(0.f, 0.f, 1.f);
			LLVector3 left_dir = up_dir % fwd_dir;
			left_dir.normalize();
			fwd_dir = left_dir % up_dir;
			LLQuaternion wQv(fwd_dir, left_dir, up_dir);

			if (is_self && mTurning)
			{
				if ((fwd_dir % pelvis_dir) * up_dir > 0.f)
				{
					gAgent.setControlFlags(AGENT_CONTROL_TURN_RIGHT);
				}
				else
				{
					gAgent.setControlFlags(AGENT_CONTROL_TURN_LEFT);
				}
			}

			// Set the root rotation, but do so incrementally so that it lags
			// in time by some fixed amount.
			F32 pelvis_lag_time = 0.f;
			if (self_in_mouselook)
			{
				pelvis_lag_time = PELVIS_LAG_MOUSELOOK;
			}
			else if (mInAir)
			{
				pelvis_lag_time = PELVIS_LAG_FLYING;
				// increase pelvis lag time when moving slowly
				pelvis_lag_time *= clamp_rescale(mSpeedAccum, 0.f, 15.f, 3.f,
												 1.f);
			}
			else
			{
				pelvis_lag_time = PELVIS_LAG_WALKING;
			}

			F32 u = llclamp(delta_time / pelvis_lag_time, 0.f, 1.f);
			mRoot->setWorldRotation(slerp(u, mRoot->getWorldRotation(), wQv));
		}
	}
	else if (mDrawable.notNull())
	{
		LLVector3 pos = mDrawable->getPosition();
		pos += getHoverOffset() * mDrawable->getRotation();
		mRoot->setPosition(pos);
		mRoot->setRotation(mDrawable->getRotation());
	}
}

// Called on both your avatar and other avatars
bool LLVOAvatar::updateCharacter()
{
	// Clear debug text
	mDebugText.clear();
	if (sShowAnimationDebug)
	{
		std::string output;
		for (LLMotionController::motion_list_t::iterator
				iter = mMotionController.getActiveMotions().begin(),
				end = mMotionController.getActiveMotions().end();
			 iter != end; ++iter)
		{
			LLMotion* motionp = *iter;
			if (motionp && motionp->getMinPixelArea() < getPixelArea())
			{
				if (motionp->getName().empty())
				{
					output = llformat("%s - %d",
									  motionp->getID().asString().c_str(),
									  (U32)motionp->getPriority());
				}
				else
				{
					output = llformat("%s - %d", motionp->getName().c_str(),
									  (U32)motionp->getPriority());
				}
				addDebugText(output);
			}
		}
	}

	if (!mIsBuilt)
	{
		return false;
	}

	bool visible = isVisible();

	// For fading out the names above heads, only let the timer run if we are
	// visible.
	if (mDrawable.notNull() && !visible)
	{
		mTimeVisible.reset();
	}

	// The rest should only be done occasionally for far away avatars

	// Sets the new value for mUpdatePeriod based on distance and various other
	// factors, also updates our (badly named: it deals with both actual
	// visibility and need to update this frame) "visible" boolean.
	computeUpdatePeriod(visible);

	if (!isPuppetAvatar())
	{
		// Change animation time quanta based on avatar render load. Not
		// for puppet avatars since it breaks their animations !  HB
		updateTimeStep();
	}

	// Do not early out for your own avatar, as we rely on your animations
	// playing reliably for example, the "turn around" animation when entering
	// customize avatar needs to trigger even when your avatar is offscreen.
	// IMPORTANT: this step must be taken *after* we changed the animation
	// quanta (see above), else animations are played ridiculously fast for
	// impostors !  HB
	if (!visible && !isSelf())
	{
		updateMotions(LLCharacter::HIDDEN_UPDATE);
		return false;
	}

	if (getParent())
	{
		if (!mIsSitting)
		{
			if (isSelf())
			{
				LL_DEBUGS("AgentSit") << "Sitting agent on parent" << LL_ENDL;
			}
			sitOnObject((LLViewerObject*)getParent());
		}
	}
	else if (mIsSitting)
	{
		if (!isMotionActive(ANIM_AGENT_SIT_GROUND_CONSTRAINED))
		{
			if (isSelf())
			{
				LL_DEBUGS("AgentSit") << "No parent and not sat on ground: unistting agent."
									  << LL_ENDL;
			}
			getOffObject();
		}
	}

	// Create local variables in world coords for region position values
	LLVector3 xy_vel = getVelocity();
	xy_vel.mV[VZ] = 0.f;
	F32 speed = xy_vel.length();

	// Remembering the value here prevents a display glitch if the animation
	// gets toggled during this update.
	bool sat_on_ground = isMotionActive(ANIM_AGENT_SIT_GROUND_CONSTRAINED);

	// This does a bunch of state updating, including figuring out whether av
	// is in the air, setting mRoot position and rotation.
	updateRootPositionAndRotation(speed, sat_on_ground);

	// Update character motions

	// Store data relevant to motions
	mSpeed = speed;

	// Update animations
	if (mSpecialRenderMode == 1) // Animation Preview
	{
		updateMotions(LLCharacter::FORCE_UPDATE);
	}
	else
	{
		updateMotions(LLCharacter::NORMAL_UPDATE);
	}

	// Special handling for sitting on ground.
	if (!getParent() && (mIsSitting || sat_on_ground))
	{
		F32 off_z = LLVector3d(getHoverOffset()).mdV[VZ];
		if (off_z != 0.f)
		{
			LLVector3 pos = mRoot->getWorldPosition();
			pos.mV[VZ] += off_z;
			mRoot->touch();
			mRoot->setWorldPosition(pos);
		}
	}

	// Update head position
	updateHeadOffset();

	if (!mIsDummy)
	{
		// Generates footstep sounds when feet hit the ground
		updateFootstepSounds();
	}

	mRoot->updateWorldMatrixChildren();

	if (!mDebugText.size() && mText.notNull())
	{
		mText->markDead();
		mText = NULL;
	}
	else if (mDebugText.size())
	{
		setDebugText(mDebugText);
	}

	// Mesh vertices need to be reskinned
	mNeedsSkin = true;

	return true;
}

void LLVOAvatar::updateHeadOffset()
{
	// Since we only care about Z, just grab one of the eyes
	LLVector3 midEyePt = mEyeLeftp->getWorldPosition();
	midEyePt -= mDrawable.notNull() ? mDrawable->getWorldPosition()
									: mRoot->getWorldPosition();
	midEyePt.mV[VZ] = llmax(-mPelvisToFoot + gViewerCamera.getNear(),
							midEyePt.mV[VZ]);

	if (mDrawable.notNull())
	{
		midEyePt = midEyePt * ~mDrawable->getWorldRotation();
	}
	if (mIsSitting)
	{
		mHeadOffset = midEyePt;
	}
	else
	{
		F32 u = llmax(0.f, HEAD_MOVEMENT_AVG_TIME - 1.f / gFPSClamped);
		mHeadOffset = lerp(midEyePt, mHeadOffset,  u);
	}
}

void LLVOAvatar::postPelvisSetRecalc()
{
	mRoot->updateWorldMatrixChildren();
	computeBodySize();
	dirtyMesh(2);
	updateHeadOffset();
}

void LLVOAvatar::updateVisibility()
{
	bool visible = false;

	if (isUIAvatar())
	{
		visible = true;
	}
	else if (mDrawable.notNull())
	{
		visible = !mDrawable->getSpatialGroup() ||
				  mDrawable->getSpatialGroup()->isVisible();

		if (isSelf())
		{
			if (!gAgentWearables.areWearablesLoaded())
			{
				visible = false;
			}
		}
		else if (!mFirstAppearanceMessageReceived)
		{
			visible = false;
		}

		if (sDebugInvisible)
		{
			llinfos << "Updating visibility for avatar ";
			std::string name = getFullname(true);
			if (name.empty())
			{
				llcont << this;
			}
			else
			{
				llcont << name;
			}
			llcont << ": " << (visible ? "Visible" :"Not visible");
#if 0
			if (avatar_in_frustum)
			{
				llcont << " - Avatar in frustum";
			}
			else
			{
				llcont << " - Avatar not in frustum";
			}
			if (gViewerCamera.sphereInFrustum(sel_pos_agent, 2.f))
			{
				llcont << " - Sel pos visible - SPA: " << sel_pos_agent;
			}
			if (gViewerCamera.sphereInFrustum(wrist_right_pos_agent, 0.2f))
			{
				llcont << " - Wrist pos visible - WPA: "
					   << wrist_right_pos_agent;
			}
			if (gViewerCamera.sphereInFrustum(getPositionAgent(),
											  getMaxScale() * 2.f))
			{
				llcont << " - Agent visible";
			}
#endif
			llcont << " - Agent position: " << getPositionAgent() << llendl;

			for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count;
				 ++i)
			{
				LLViewerJointAttachment* attachment = mAttachedObjectsVector[i].second;
				if (!attachment) continue;	// Paranoia
				std::string name = LLTrans::getString(attachment->getName());

				LLViewerObject* object = mAttachedObjectsVector[i].first;
				if (object && object->mDrawable &&
					object->mDrawable->isVisible())
				{
					llinfos << name << " visible" << llendl;
				}
				else
				{
					llinfos << name << " not visible at "
							<< mDrawable->getWorldPosition() << " and radius "
							<< mDrawable->getRadius() << llendl;
				}
			}
		}
	}

	if (!visible && mVisible)
	{
		mMeshInvisibleTime.reset();
	}

	if (visible)
	{
		if (!mMeshValid)
		{
			restoreMeshData();
		}
	}
	else
	{
		if (mMeshValid &&
			(isPuppetAvatar() ||
			 mMeshInvisibleTime.getElapsedTimeF32() > TIME_BEFORE_MESH_CLEANUP))
		{
			releaseMeshData();
		}
#if 0	// This breaks off-screen chat bubbles
		deleteNameTag();
#endif
	}

	mVisible = visible;
}

bool LLVOAvatar::shouldAlphaMask()
{
	// Do not alpha mask when highlight transparent textures.
	return !LLDrawPoolAlpha::sShowDebugAlpha &&
		   !LLDrawPoolAvatar::sSkipTransparent;
}

U32 LLVOAvatar::renderSkinned()
{
	U32 num_indices = 0;

	if (!mIsBuilt || mDrawable.isNull())
	{
		return num_indices;
	}

	LLFace* face = mDrawable->getFace(0);

	bool needs_rebuild = !face || !face->getVertexBuffer() ||
						 mDrawable->isState(LLDrawable::REBUILD_GEOMETRY);

	if (needs_rebuild || mDirtyMesh)
	{
		// LOD changed or new mesh created, allocate new vertex buffer if
		// needed
		if (needs_rebuild || mDirtyMesh >= 2 || mVisibilityRank <= 4)
		{
			updateMeshData();
			mDirtyMesh = 0;
			mNeedsSkin = true;
			mDrawable->clearState(LLDrawable::REBUILD_GEOMETRY);
		}
	}

	if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR) <= 0)
	{
		if (mNeedsSkin)
		{
			// Generate animated mesh

			LLViewerJoint* lower_mesh = getViewerJoint(MESH_ID_LOWER_BODY);
			if (lower_mesh)
			{
				lower_mesh->updateJointGeometry();
			}

			LLViewerJoint* upper_mesh = getViewerJoint(MESH_ID_UPPER_BODY);
			if (upper_mesh)
			{
				upper_mesh->updateJointGeometry();
			}

			LLViewerJoint* skirt_mesh = getViewerJoint(MESH_ID_SKIRT);
			if (skirt_mesh && isWearingWearableType(LLWearableType::WT_SKIRT))
			{
				skirt_mesh->updateJointGeometry();
			}

			LLViewerJoint* eyelash_mesh = getViewerJoint(MESH_ID_EYELASH);
			LLViewerJoint* head_mesh = getViewerJoint(MESH_ID_HEAD);
			LLViewerJoint* hair_mesh = getViewerJoint(MESH_ID_HAIR);
			if (!isSelf() || gAgent.needsRenderHead() || LLPipeline::sShadowRender)
			{
				if (eyelash_mesh)
				{
					eyelash_mesh->updateJointGeometry();
				}
				if (head_mesh)
				{
					head_mesh->updateJointGeometry();
				}
				if (hair_mesh)
				{
					hair_mesh->updateJointGeometry();
				}
			}
			mNeedsSkin = false;
			mLastSkinTime = gFrameTimeSeconds;

			face = mDrawable->getFace(0);
			if (face)
			{
				LLVertexBuffer* vb = face->getVertexBuffer();
				if (vb)
				{
					vb->unmapBuffer();
				}
			}
		}
	}
	else
	{
		mNeedsSkin = false;
	}

	if (sDebugInvisible)
	{
		llinfos << "Avatar ";
		std::string name = getFullname(true);
		if (name.empty())
		{
			llcont << this;
		}
		else
		{
			llcont << name;
		}
		llcont << " in render. ";

		if (!mIsBuilt)
		{
			llcont << "Not built.";
		}
		else if (!gAgent.needsRenderAvatar())
		{
			llcont << "Does not need render.";
		}
		else
		{
			llcont << "Rendering.";
		}
		llcont << llendl;
	}

	if (!mIsBuilt)
	{
		return num_indices;
	}

	if (isSelf() && !gAgent.needsRenderAvatar())
	{
		return num_indices;
	}

	// Render all geometry attached to the skeleton

	bool first_pass = true;
	if (!LLDrawPoolAvatar::sSkipOpaque)
	{
		if (mIsDummy && isTextureVisible(TEX_UPPER_BAKED))
		{
			LLViewerJoint* hair_mesh = getViewerJoint(MESH_ID_HAIR);
			if (hair_mesh)
			{
				num_indices += hair_mesh->render(mAdjustedPixelArea, true,
												 mIsDummy);
			}
			first_pass = false;
		}
		bool jelly_doll = isVisuallyMuted() || isUIAvatar();
		if (!isSelf() || gAgent.needsRenderHead() || LLPipeline::sShadowRender)
		{
			if (jelly_doll || isTextureVisible(TEX_HEAD_BAKED))
			{
				LLViewerJoint* head_mesh = getViewerJoint(MESH_ID_HEAD);
				if (head_mesh)
				{
					num_indices += head_mesh->render(mAdjustedPixelArea,
													 first_pass, mIsDummy);
				}
				first_pass = false;
			}
		}
		if (jelly_doll || isTextureVisible(TEX_UPPER_BAKED))
		{
			LLViewerJoint* upper_mesh = getViewerJoint(MESH_ID_UPPER_BODY);
			if (upper_mesh)
			{
				num_indices += upper_mesh->render(mAdjustedPixelArea,
												  first_pass, mIsDummy);
			}
			first_pass = false;
		}

		if (jelly_doll || isTextureVisible(TEX_LOWER_BAKED))
		{
			LLViewerJoint* lower_mesh = getViewerJoint(MESH_ID_LOWER_BODY);
			if (lower_mesh)
			{
				num_indices += lower_mesh->render(mAdjustedPixelArea,
												  first_pass, mIsDummy);
			}
			first_pass = false;
		}
	}

	if (!LLDrawPoolAvatar::sSkipTransparent || LLPipeline::sImpostorRender)
	{
		LLGLState blend(GL_BLEND, mIsDummy ? GL_FALSE : GL_TRUE);
		num_indices += renderTransparent(first_pass);
	}

	return num_indices;
}

U32 LLVOAvatar::renderTransparent(bool first_pass)
{
	if (isPuppetAvatar())
	{
		return 0;
	}

	U32 num_indices = 0;
	if (isWearingWearableType(LLWearableType::WT_SKIRT) &&
		(isUIAvatar() || isTextureVisible(TEX_SKIRT_BAKED)))
	{
		LLViewerJoint* skirt_mesh = getViewerJoint(MESH_ID_SKIRT);
		if (skirt_mesh)
		{
			gGL.flush();
			num_indices += skirt_mesh->render(mAdjustedPixelArea, false);
			gGL.flush();
		}
		first_pass = false;
	}

	if (!isSelf() || gAgent.needsRenderHead() || LLPipeline::sShadowRender)
	{
		if (LLPipeline::sImpostorRender)
		{
			gGL.flush();
		}
		if (isTextureVisible(TEX_HEAD_BAKED))
		{
			LLViewerJoint* eyelash_mesh = getViewerJoint(MESH_ID_EYELASH);
			if (eyelash_mesh)
			{
				num_indices += eyelash_mesh->render(mAdjustedPixelArea,
													first_pass, mIsDummy);
			}
			first_pass = false;
		}
		// Cannot test for baked hair being defined, since that won't always be
		// the case (not all viewers send baked hair)
		// TODO: 1.25 will be able to switch this logic back to calling
		// isTextureVisible()
		if (LLDrawPoolAlpha::sShowDebugAlpha ||
			(getImage(TEX_HAIR_BAKED, 0) &&
			 getImage(TEX_HAIR_BAKED, 0)->getID() != IMG_INVISIBLE))
		{
			LLViewerJoint* hair_mesh = getViewerJoint(MESH_ID_HAIR);
			if (hair_mesh)
			{
				num_indices += hair_mesh->render(mAdjustedPixelArea,
												 first_pass, mIsDummy);
			}
			first_pass = false;
		}
		if (LLPipeline::sImpostorRender)
		{
			gGL.flush();
		}
	}

	return num_indices;
}

U32 LLVOAvatar::renderRigid()
{
//MK
	if (isSelf() && gRLenabled && gRLInterface.mVisionRestricted &&
		!gRLInterface.mRenderLimitRenderedThisFrame &&
		!LLPipeline::sRenderDeferred && isFullyLoaded())
	{
		LL_TRACY_TIMER(TRC_RLV_RENDER_LIMITS);
		// Possibly draw a big black sphere around our avatar if the camera
		// render is limited
		gRLInterface.drawRenderLimit(false);
	}
//mk

	if (isSelf() && (!gAgent.needsRenderAvatar() || !gAgent.needsRenderHead()))
	{
		return 0;
	}

	if (!mIsBuilt)
	{
		return 0;
	}

	U32 num_indices = 0;

	if (isTextureVisible(TEX_EYES_BAKED) || isUIAvatar())
	{
		LLViewerJoint* jointp = getViewerJoint(MESH_ID_EYEBALL_LEFT);
		if (jointp)
		{
			num_indices += jointp->render(mAdjustedPixelArea, true, mIsDummy);
		}
		jointp = getViewerJoint(MESH_ID_EYEBALL_RIGHT);
		if (jointp)
		{
			num_indices += jointp->render(mAdjustedPixelArea, true, mIsDummy);
		}
	}

	return num_indices;
}

U32 LLVOAvatar::renderImpostor(LLColor4U color, S32 diffuse_channel)
{
	if (!mImpostor.isComplete())
	{
		return 0;
	}

	LLVector3 pos(getRenderPosition() + mImpostorOffset);
	LLVector3 at = pos - gViewerCamera.getOrigin();
	at.normalize();
	LLVector3 left = gViewerCamera.getUpAxis() % at;
	LLVector3 up = at % left;

	left *= mImpostorDim.mV[0];
	up *= mImpostorDim.mV[1];

	gGL.flush();

	gGL.color4ubv(color.mV);
	gGL.getTexUnit(diffuse_channel)->bind(&mImpostor);
	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3fv((pos + left - up).mV);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv((pos - left - up).mV);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv((pos - left + up).mV);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3fv((pos + left - up).mV);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv((pos - left + up).mV);
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv((pos + left + up).mV);
	}
	gGL.end(true);

	return 6;
}

bool LLVOAvatar::allTexturesCompletelyDownloaded(uuid_list_t& ids) const
{
	for (uuid_list_t::const_iterator it = ids.begin(), end = ids.end();
		 it != end; ++it)
	{
		LLViewerFetchedTexture* imagep = gTextureList.findImage(*it);
		if (imagep && imagep->getDiscardLevel() != 0)
		{
			return false;
		}
	}
	return true;
}

bool LLVOAvatar::allLocalTexturesCompletelyDownloaded() const
{
	uuid_list_t local_ids;
	collectLocalTextureUUIDs(local_ids);
	return allTexturesCompletelyDownloaded(local_ids);
}

bool LLVOAvatar::allBakedTexturesCompletelyDownloaded() const
{
	uuid_list_t baked_ids;
	collectBakedTextureUUIDs(baked_ids);
	return allTexturesCompletelyDownloaded(baked_ids);
}

void LLVOAvatar::bakedTextureOriginCounts(S32& sb_count,		// server-bake, has origin URL.
										  S32& host_count,		// host-based bake, has host.
										  S32& both_count,		// error - both host and URL set.
										  S32& neither_count)	// error - neither set.
{
	sb_count = host_count = both_count = neither_count = 0;

	uuid_list_t baked_ids;
	collectBakedTextureUUIDs(baked_ids);
	for (uuid_list_t::const_iterator it = baked_ids.begin(),
									 end = baked_ids.end();
		 it != end; ++it)
	{
		LLViewerFetchedTexture* imagep = gTextureList.findImage(*it);
		if (!imagep) continue;	// Paranoia

		bool has_url = !imagep->getUrl().empty();
		bool has_host = imagep->getTargetHost().isOk();
		if (has_url && !has_host)
		{
			++sb_count;
		}
		else if (has_host && !has_url)
		{
			++host_count;
		}
		else if (has_host && has_url)
		{
			++both_count;
		}
		else if (!has_host && !has_url)
		{
			++neither_count;
		}
	}
}

void LLVOAvatar::collectLocalTextureUUIDs(uuid_list_t& ids) const
{
	for (U32 i = 0, count = getNumTEs(); i < count; ++i)
	{
		LLWearableType::EType wearable_type =
			LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)i);
		U32 num_wearables = gAgentWearables.getWearableCount(wearable_type);

		for (U32 j = 0; j < num_wearables; ++j)
		{
			LLViewerFetchedTexture* texp =
				LLViewerTextureManager::staticCast(getImage(i, j), true);
			if (texp)
			{
				const LLUUID& id = texp->getID();
				if (id != IMG_DEFAULT && id != IMG_DEFAULT_AVATAR &&
					id != IMG_INVISIBLE)
				{
					const LLAvatarAppearanceDictionary::TextureEntry* dictp =
						gAvatarAppDictp->getTexture((ETextureIndex)i);
					if (dictp && dictp->mIsLocalTexture)
					{
						ids.emplace(id);
					}
				}
			}
		}
	}
}

void LLVOAvatar::collectBakedTextureUUIDs(uuid_list_t& ids) const
{
	for (U32 i = 0, count = getNumTEs(); i < count; ++i)
	{
		if (isIndexBakedTexture((ETextureIndex)i))
		{
			LLViewerFetchedTexture* texp =
				LLViewerTextureManager::staticCast(getImage(i, 0), true);
			if (texp)
			{
				const LLUUID& id = texp->getID();
				if (id != IMG_DEFAULT && id != IMG_DEFAULT_AVATAR &&
					id != IMG_INVISIBLE)
				{
					ids.emplace(id);
				}
			}
		}
	}
}

void LLVOAvatar::collectTextureUUIDs(uuid_list_t& ids)
{
	collectLocalTextureUUIDs(ids);
	collectBakedTextureUUIDs(ids);
}

void LLVOAvatar::releaseOldTextures()
{
	LL_FAST_TIMER(FTM_AV_RELEASE_OLD_TEX);

	// Any textures that we used to be using but are no longer using should no
	// longer be flagged as "NO_DELETE"
	uuid_list_t baked_texture_ids;
	collectBakedTextureUUIDs(baked_texture_ids);
	uuid_list_t local_texture_ids;
	collectLocalTextureUUIDs(local_texture_ids);
	uuid_list_t new_texture_ids;
	new_texture_ids.insert(baked_texture_ids.begin(), baked_texture_ids.end());
	new_texture_ids.insert(local_texture_ids.begin(), local_texture_ids.end());

	for (uuid_list_t::iterator it = mTextureIDs.begin(),
							   end = mTextureIDs.end();
		 it != end; ++it)
	{
		if (new_texture_ids.count(*it))
		{
			LLViewerFetchedTexture* imagep = gTextureList.findImage(*it);
			if (imagep)
			{
				if (imagep->getTextureState() == LLGLTexture::NO_DELETE)
				{
					// This will allow the texture to be deleted if not in use.
					imagep->forceActive();

					// This resets the clock to texture being flagged as
					// unused, preventing the texture from being deleted
					// immediately. If other avatars or objects are using it,
					// it can still be flagged no-delete by them.
					imagep->forceUpdateBindStats();
				}
			}
		}
	}
	mTextureIDs = new_texture_ids;
}

void LLVOAvatar::updateTextures()
{
	if (mIsDummy)
	{
		return;
	}

	bool render_avatar = true;
	if (!isSelf())
	{
		if (!isVisible())
		{
			return;
		}
		else
		{
			render_avatar = !mCulled;
		}
	}

	LL_FAST_TIMER(FTM_AV_UPDATE_TEXTURES);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	std::vector<bool> layer_baked;
	bool is_editing_appearance = isSelf() && isUsingLocalAppearance();
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		bool baked = !is_editing_appearance &&
					 isTextureDefined(mBakedTextureDatas[i].mTextureIndex);
		if (isSelf())	// There is no texture stats for non-self avatars
		{
			layer_baked.push_back(baked);
		}
		if (baked && render_avatar && !gGLManager.mIsDisabled &&
			!mBakedTextureDatas[i].mIsLoaded)
		{
			// Bind the texture so that it will be decoded: slightly
			// inefficient, we can short-circuit this if we have to.
			unit0->bind(getImage(mBakedTextureDatas[i].mTextureIndex, 0));
		}
	}

	mMaxPixelArea = 0.f;
	mMinPixelArea = 99999999.f;
	mHasGrey = false; // debug
	for (U32 texture_index = 0, count = getNumTEs(); texture_index < count;
		 ++texture_index)
	{
		LLWearableType::EType wearable_type =
			LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)texture_index);
		U32 num_wearables = gAgentWearables.getWearableCount(wearable_type);
		const LLTextureEntry* te = getTE(texture_index);
		const F32 texel_area_ratio = te ? fabs(te->getScaleS() * te->getScaleT())
										: 1.f;
		LLViewerFetchedTexture* imagep = NULL;
		if (isSelf())	// There is no texture stats for non-self avatars
		{
			for (U32 wearable_index = 0; wearable_index < num_wearables;
				 ++wearable_index)
			{
				imagep =
					LLViewerTextureManager::staticCast(getImage(texture_index,
																wearable_index),
													   true);
				if (!imagep) continue;

				const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
					gAvatarAppDictp->getTexture((ETextureIndex)texture_index);
				if (!t_dict || !t_dict->mIsLocalTexture) continue;

				const EBakedTextureIndex bt_idx = t_dict->mBakedTextureIndex;
				addLocalTextureStats((ETextureIndex)texture_index, imagep,
									 texel_area_ratio, render_avatar,
									 layer_baked[bt_idx]);
			}
		}
		if (isIndexBakedTexture((ETextureIndex)texture_index) && render_avatar)
		{
			imagep =
				LLViewerTextureManager::staticCast(getImage(texture_index, 0),
												   true);
			// Spam if this is a baked texture, not set to default image, without
			// valid host info
			if (isIndexBakedTexture((ETextureIndex)texture_index) &&
				imagep->getID() != IMG_DEFAULT_AVATAR &&
				imagep->getID() != IMG_INVISIBLE &&
				!isUsingServerBakes() && !imagep->getTargetHost().isOk())
			{
				llwarns << "No host for texture " << imagep->getID()
						<< " for avatar "
						<< (isSelf() ? "<myself>" : getFullname(true))
						<< " on host " << getRegion()->getHost() << llendl;
			}

			addBakedTextureStats(imagep, mPixelArea, texel_area_ratio);
		}
	}

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
	{
		setDebugText(llformat("%4.0f:%4.0f", sqrtf(mMinPixelArea),
							  sqrtf(mMaxPixelArea)));
	}
}

void LLVOAvatar::addLocalTextureStats(ETextureIndex idx,
									  LLViewerFetchedTexture* imagep,
									  F32 texel_area_ratio,
									  bool render_avatar,
									  bool covered_by_baked)
{
	// No local texture stats for non-self avatars
	return;
}

// need to call updateTextures() at least every 32 frames:
constexpr S32 MAX_TEXTURE_UPDATE_INTERVAL = 64;

void LLVOAvatar::checkTextureLoading()
{
	LL_FAST_TIMER(FTM_AV_CHECK_TEX_LOADING);

	constexpr F32 MAX_INVISIBLE_WAITING_TIME = 15.f; // seconds

	bool pause = !isVisible();
	if (!pause)
	{
		mInvisibleTimer.reset();
	}
	if (mLoadedCallbacksPaused == pause)
	{
		return;
	}

	// When is self or no callbacks. Note: this list for self is always empty.
	if (mCallbackTextureList.empty())
	{
		mLoadedCallbacksPaused = pause;
		return; // Nothing to check.
	}

	if (pause &&
		mInvisibleTimer.getElapsedTimeF32() < MAX_INVISIBLE_WAITING_TIME)
	{
		return; // Have not been invisible for enough time.
	}

	for	(uuid_list_t::iterator iter = mCallbackTextureList.begin(),
							   end = mCallbackTextureList.end();
		 iter != end; ++iter)
	{
		LLViewerFetchedTexture* tex = gTextureList.findImage(*iter);
		if (tex)
		{
			if (pause)
			{	// Pause texture fetching.
				tex->pauseLoadedCallbacks(&mCallbackTextureList);

				// Set to terminate texture fetching after
				// MAX_TEXTURE_UPDATE_INTERVAL frames.
				tex->setMaxVirtualSizeResetInterval(MAX_TEXTURE_UPDATE_INTERVAL);
				tex->resetMaxVirtualSizeResetCounter();
			}
			else	// Unpause
			{
				tex->unpauseLoadedCallbacks(&mCallbackTextureList);
				// Jump start the fetching again
				constexpr F32 START_AREA = 100.f;
				tex->addTextureStats(START_AREA);
			}
		}
	}

	if (!pause)
	{
		releaseOldTextures();
		updateTextures();	// Refresh texture stats.
	}
	mLoadedCallbacksPaused = pause;
}

void LLVOAvatar::addBakedTextureStats(LLViewerFetchedTexture* imagep,
									  F32 pixel_area,
									  F32 texel_area_ratio)
{
	imagep->resetTextureStats();
	imagep->setMaxVirtualSizeResetInterval(S32_MAX);
	imagep->resetMaxVirtualSizeResetCounter();

	mMaxPixelArea = llmax(pixel_area, mMaxPixelArea);
	mMinPixelArea = llmin(pixel_area, mMinPixelArea);
	imagep->addTextureStats(pixel_area / texel_area_ratio);

	S32 boost_level;
	F32 added_prio;
	if (isSelf())
	{
		boost_level = LLGLTexture::BOOST_AVATAR_SELF;
		added_prio = SELF_ADDITIONAL_PRI;
#if !LL_IMPLICIT_SETNODELETE
		imagep->setNoDelete();
#endif
	}
	else
	{
		boost_level = LLGLTexture::BOOST_AVATAR;
		added_prio = ADDITIONAL_PRI;
	}
	imagep->setAdditionalDecodePriority(added_prio);
	imagep->setBoostLevel(boost_level);
}

//virtual
void LLVOAvatar::setImage(U8 te, LLViewerTexture* imagep, U32 index)
{
	setTEImage(te, imagep);
}

//virtual
LLViewerTexture* LLVOAvatar::getImage(U8 te, U32) const
{
	return getTEImage(te);
}

std::string LLVOAvatar::getImageURL(U8 te, const LLUUID& id)
{
	llassert(isIndexBakedTexture(ETextureIndex(te)));
	std::string url;
	if (isUsingServerBakes())
	{
		if (sAgentAppearanceServiceURL.empty())
		{
			// Probably a server-side issue if we get here:
			llwarns << "AgentAppearanceServiceURL not set - Baked texture requests will fail"
					 << llendl;
		}
		else
		{
			const LLAvatarAppearanceDictionary::TextureEntry* tep =
				gAvatarAppDictp->getTexture((ETextureIndex)te);
			if (tep && id.notNull())
			{
				url = sAgentAppearanceServiceURL + "texture/" +
					  mID.asString() + "/" + tep->mDefaultImageName + "/" +
					  id.asString();
				LL_DEBUGS("Avatar") << "Baked texture url: " << url << LL_ENDL;
			}
		}
	}
	return url;
}

void LLVOAvatar::resolveHeightAgent(const LLVector3& in_pos_agent,
									LLVector3& out_pos_agent,
									LLVector3& out_norm)
{
	LLVector3d in_pos_global, out_pos_global;

	in_pos_global = gAgent.getPosGlobalFromAgent(in_pos_agent);
	resolveHeightGlobal(in_pos_global, out_pos_global, out_norm);
	out_pos_agent = gAgent.getPosAgentFromGlobal(out_pos_global);
}

void LLVOAvatar::resolveRayCollisionAgent(const LLVector3d start_pt,
										  const LLVector3d end_pt,
										  LLVector3d& out_pos,
										  LLVector3& out_norm)
{
	LLViewerObject* obj;
	gWorld.resolveStepHeightGlobal(this, start_pt, end_pt, out_pos, out_norm,
								   &obj);
}

void LLVOAvatar::resolveHeightGlobal(const LLVector3d& in_pos,
									 LLVector3d& out_pos,
									 LLVector3& out_norm)
{
	const LLVector3d z_vec(0.f, 0.f, 0.5f);
	LLVector3d p0 = in_pos + z_vec;
	LLVector3d p1 = in_pos - z_vec;
	LLViewerObject* obj;
	gWorld.resolveStepHeightGlobal(this, p0, p1, out_pos, out_norm, &obj);
	if (!obj)
	{
		mStepOnLand = true;
		mStepMaterial = 0;
		mStepObjectVelocity.set(0.f, 0.f, 0.f);
	}
	else
	{
		mStepOnLand = false;
		mStepMaterial = obj->getMaterial();

		// We want the primitive velocity, not our velocity (which actually
		// subtracts the step object velocity)...
		LLVector3 angularVelocity = obj->getAngularVelocity();
		LLVector3 relativePos = gAgent.getPosAgentFromGlobal(out_pos) -
								obj->getPositionAgent();

		LLVector3 linearComponent = angularVelocity % relativePos;
		mStepObjectVelocity = obj->getVelocity() + linearComponent;
	}
}

const LLUUID& LLVOAvatar::getStepSound() const
{
	if (gIsInSecondLife)
	{
		return mStepOnLand ? sStepSoundOnLand : sStepSounds[mStepMaterial];
	}
	// Just one collision sound available in OpenSIM...
	return SND_OPENSIM_COLLISION;
}

void LLVOAvatar::processAnimationStateChanges()
{
	// *BUG ?  AGENT_WALK_ANIMS does not contains the new walk/run anims
	// neither the female walk anim: is this normal ?   HB
	if (isAnyAnimationSignaled(AGENT_WALK_ANIMS, NUM_AGENT_WALK_ANIMS))
	{
		// Do not perform adjustments on dummy/puppets; this would break the
		// walk anims !  HB
		if (mEnableDefaultMotions)
		{
			startMotion(ANIM_AGENT_WALK_ADJUST);
		}
		stopMotion(ANIM_AGENT_FLY_ADJUST);
	}
	else if (mInAir && !mIsSitting)
	{
		stopMotion(ANIM_AGENT_WALK_ADJUST);
		if (mEnableDefaultMotions)
		{
			startMotion(ANIM_AGENT_FLY_ADJUST);
		}
	}
	else
	{
		stopMotion(ANIM_AGENT_WALK_ADJUST);
		stopMotion(ANIM_AGENT_FLY_ADJUST);
	}

	if (isAnyAnimationSignaled(AGENT_GUN_AIM_ANIMS, NUM_AGENT_GUN_AIM_ANIMS))
	{
		if (mEnableDefaultMotions)
		{
			startMotion(ANIM_AGENT_TARGET);
		}
		stopMotion(ANIM_AGENT_BODY_NOISE);
	}
	else
	{
		stopMotion(ANIM_AGENT_TARGET);
		if (mEnableDefaultMotions)
		{
			startMotion(ANIM_AGENT_BODY_NOISE);
		}
	}

	anim_it_t end_signaled_anims = mSignaledAnimations.end();

	// Clear all current animations
	for (anim_it_t it = mPlayingAnimations.begin(),
				   end = mPlayingAnimations.end();
		 it != end; )
	{
		const LLUUID& id = it->first;
		anim_it_t found_anim = mSignaledAnimations.find(id);
		// Playing, but not signaled, so stop
		if (found_anim == end_signaled_anims)
		{
			processSingleAnimationStateChange(id, false);
			mPlayingAnimations.erase(it++);
		}
		else
		{
			++it;
		}
	}

	// Start up all new anims
	for (anim_it_t it = mSignaledAnimations.begin(); it != end_signaled_anims;
		 ++it)
	{
		const LLUUID& id = it->first;
		anim_it_t found_anim = mPlayingAnimations.find(id);
		// Signaled but not playing, or different sequence id, start motion
		if (found_anim == mPlayingAnimations.end() ||
			found_anim->second != it->second)
		{
			if (processSingleAnimationStateChange(id, true))
			{
				mPlayingAnimations.emplace(id, it->second);
			}
		}
	}

	// Clear source information for animations which have been stopped
	if (isSelf())
	{
		for (anim_src_map_it_t it = mAnimationSources.begin(),
							   end = mAnimationSources.end();
			 it != end; )
		{
			if (mSignaledAnimations.find(it->second) == end_signaled_anims)
			{
				mAnimationSources.erase(it++);
			}
			else
			{
				++it;
			}
		}
	}
}

bool LLVOAvatar::processSingleAnimationStateChange(const LLUUID& anim_id,
												   bool start)
{
	if (anim_id.isNull())
	{
		// Happens for hand animations (Bento mesh avatar with hand joints and
		// no hands anim defined ?). Just abort.
		return false;
	}

	// Keep track of bad assets, to avoid repeated "Failed to start motion"
	// warnings for them. HB
	static uuid_list_t bad_motions;
	if (!isSelf() && bad_motions.count(anim_id))
	{
		return false;
	}

	// SL-402: with the ability to animate the position of joints that affect
	// the body size calculation, computed body size can get stale much more
	// easily. Simplest fix is to update it frequently.
	computeBodySize();

	if (!start) // Stop animation
	{
		if (anim_id == ANIM_AGENT_SIT_GROUND_CONSTRAINED)
		{
			mIsSitting = false;
//MK
			if (gRLenabled && isSelf())
			{
				if (gRLInterface.mContainsUnsit)
				{
					gRLInterface.notify("unsat ground illegally");
				}
				else
				{
					gRLInterface.notify("unsat ground legally");
				}
			}
//mk
		}
		stopMotion(anim_id);
		return true;
	}

	if (anim_id == ANIM_AGENT_TYPE)
	{
		if (gAudiop && gSavedSettings.getBool("UISndTypingEnable"))
		{
			LLVector3d char_pos_global =
				gAgent.getPosGlobalFromAgent(getCharacterPosition());
			if (gViewerParcelMgr.canHearSound(char_pos_global) &&
				(mCachedMuteFlags & LLMute::flagObjectSounds))
			{
				LLUUID sound_id =
					LLUUID(gSavedSettings.getString("UISndTyping"));
#if 0			// RN: enable this to play on typing sound at fixed volume
				// once sound engine is fixed to support both spatialized and
				// non-spatialized instances of the same sound
				if (isSelf())
				{
					gAudiop->triggerSound(sound_id, 1.f,
										  LLAudioEngine::AUDIO_TYPE_UI);
				}
				else
#endif
				{
					gAudiop->triggerSound(sound_id, mID, 1.f,
										  LLAudioEngine::AUDIO_TYPE_SFX,
										  char_pos_global);
				}
			}
		}
	}
	else if (anim_id == ANIM_AGENT_SIT_GROUND_CONSTRAINED)
	{
		mIsSitting = true;
//MK
		if (gRLenabled && isSelf())
		{
			gRLInterface.notify("sat ground legally");
		}
//mk
	}

	if (startMotion(anim_id))
	{
		return true;
	}

	llwarns << "Failed to start motion: " << anim_id << llendl;
	// If it was supposed to play on our avatar, send a stop request to the
	// server to inform surrounding avatars and scripts we do not play that
	// bogus animation.
	if (isSelf())
	{
		llinfos << "Sending ANIM_REQUEST_STOP for motion: " << anim_id
				<< llendl;
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}
	else
	{
		bad_motions.emplace(anim_id);
	}

	return false;
}

bool LLVOAvatar::isAnyAnimationSignaled(const LLUUID* anim_array,
										S32 num_anims) const
{
	anim_map_t::const_iterator end_signaled_anims = mSignaledAnimations.end();
	for (S32 i = 0; i < num_anims; ++i)
	{
		const LLUUID& id = anim_array[i];
		if (mSignaledAnimations.find(id) != end_signaled_anims)
		{
			return true;
		}
	}
	return false;
}

void LLVOAvatar::resetAnimations()
{
	LLKeyframeMotion::flushKeyframeCache();
	flushAllMotions();
}

LLUUID LLVOAvatar::remapMotionID(const LLUUID& id)
{
	if (mIsDummy)
	{
		// Do not remap animations for dummy avatars or puppets. HB
		return id;
	}

	static LLCachedControl<bool> use_new_anims(gSavedSettings,
											   "UseNewWalkRun");
	// Female anims for female avatars
	if (getSex() == SEX_FEMALE)
	{
		if (id == ANIM_AGENT_WALK)
		{
			return use_new_anims ? ANIM_AGENT_FEMALE_WALK_NEW
								 : ANIM_AGENT_FEMALE_WALK;
		}
		if (id == ANIM_AGENT_RUN)
		{
			return use_new_anims ? ANIM_AGENT_FEMALE_RUN_NEW : ANIM_AGENT_RUN;
		}
		if (id == ANIM_AGENT_SIT)
		{
			return ANIM_AGENT_SIT_FEMALE;
		}
	}
	// Male avatar below this line.
	else if (id == ANIM_AGENT_SIT_FEMALE)
	{
		// Keep in sync with setSex() related code (viewer controls sit's sex)
		return ANIM_AGENT_SIT;
	}
	else if (use_new_anims)
	{
		if (id == ANIM_AGENT_WALK)
		{
			return ANIM_AGENT_WALK_NEW;
		}
		if (id == ANIM_AGENT_RUN)
		{
			return ANIM_AGENT_RUN_NEW;
		}
	}
	return id;	// No change
}

// 'id' is the asset if of the animation to start, 'time_offset' is the offset
// into the animation at which to start playing
bool LLVOAvatar::startMotion(const LLUUID& id, F32 time_offset)
{
	LLUUID remap_id = remapMotionID(id);

	LL_DEBUGS("Animation") << "Motion requested: "
						   << gAnimLibrary.animationName(id);
	if (id != remap_id)
	{
		LL_CONT << " - Remapped as: " << gAnimLibrary.animationName(remap_id);
	}
	LL_CONT << LL_ENDL;

	if (isSelf() && remap_id == ANIM_AGENT_AWAY)
	{
		gAgent.setAFK();
	}

	return LLCharacter::startMotion(remap_id, time_offset);
}

bool LLVOAvatar::stopMotion(const LLUUID& id, bool stop_immediate)
{
	LLUUID remap_id = remapMotionID(id);

	LL_DEBUGS("Animation") << "Motion requested: "
						   << gAnimLibrary.animationName(id);
	if (id != remap_id)
	{
		LL_CONT << " - Remapped as: " << gAnimLibrary.animationName(remap_id);
	}
	LL_CONT << LL_ENDL;

	if (isSelf())
	{
		gAgent.onAnimStop(remap_id);
	}

	return LLCharacter::stopMotion(remap_id, stop_immediate);
}

void LLVOAvatar::addDebugText(const std::string& text)
{
	mDebugText.append(1, '\n');
	mDebugText.append(text);
}

// RN: avatar joints are multi-rooted to include screen-based attachments
LLJoint* LLVOAvatar::getJoint(U32 key)
{
	LLJoint* jointp = NULL;

	joint_map_t::iterator iter = mJointMap.find(key);
	if (iter == mJointMap.end() || iter->second == NULL)
	{
		// Search for joint and cache it in lookup table
		jointp = mRoot->findJoint(key);
		mJointMap[key] = jointp;
	}
	else
	{
		// Return cached pointer
		jointp = iter->second;
	}

	return jointp;
}

LLVOAvatar::rtf_cache_it_t LLVOAvatar::initRiggedMatrixCache(const LLMeshSkinInfo* skin,
															 U32& count)
{
	const LLUUID& mesh_id = skin->mMeshID;
	rtf_cache_it_t iter = mRiggedMatrixDataCache.find(mesh_id);
	if (iter == mRiggedMatrixDataCache.end())
	{
		// No entry: create a new one for that mesh.
		iter = mRiggedMatrixDataCache.emplace(mesh_id, new RiggedMatrix).first;
	}
	else if (iter->second.isNull())
	{
		// NULL entry (should not happen): affect a new one to that mesh.
		llwarns << "NULL entry in cache for mesh " << mesh_id << llendl;
		iter->second = new RiggedMatrix;
	}
	else if (iter->second->mFrameNumber == gFrameCount &&
			 !isEditingAppearance())
	{
		// Entry exists and is still valid, so return its data at once
		count = iter->second->mCount;
		return iter;
	}

	LLPointer<RiggedMatrix>& rigmatp = iter->second;

	// Stamp the cache entry with the current frame number
	rigmatp->mFrameNumber = gFrameCount;

	// Fill-up the matrix

	LLMatrix4a* mat = rigmatp->mMatrix4a;
	count = rigmatp->mCount =
		LLSkinningUtil::initSkinningMatrixPalette(mat, skin, this);
	U32 idx = 0;
	F32* mp = rigmatp->mMatrix;
	for (U32 i = 0; i < count; ++i)
	{
		const F32* m = mat[i].mMatrix[0].getF32ptr();

		mp[idx++] = m[0];
		mp[idx++] = m[1];
		mp[idx++] = m[2];
		mp[idx++] = m[12];

		mp[idx++] = m[4];
		mp[idx++] = m[5];
		mp[idx++] = m[6];
		mp[idx++] = m[13];

		mp[idx++] = m[8];
		mp[idx++] = m[9];
		mp[idx++] = m[10];
		mp[idx++] = m[14];
	}

	return iter;
}

const F32* LLVOAvatar::getRiggedMatrix(const LLMeshSkinInfo* skin, U32& count)
{
	return initRiggedMatrixCache(skin, count)->second->mMatrix;
}

const LLMatrix4a* LLVOAvatar::getRiggedMatrix4a(const LLMeshSkinInfo* skin,
												U32& count)
{
	return initRiggedMatrixCache(skin, count)->second->mMatrix4a;
}

// If viewer object is a rigged mesh, set the mesh id and return true.
// Otherwise, null out the id and return false.
//static
bool LLVOAvatar::getRiggedMeshID(LLViewerObject* vobj, LLUUID& mesh_id)
{
	mesh_id.setNull();

	// If a VO has a skin that we will reset the joint positions to their
	// default
	if (vobj && vobj->mDrawable)
	{
		LLVOVolume* vvo = vobj->mDrawable->getVOVolume();
		if (vvo)
		{
			const LLMeshSkinInfo* skindatap = vvo->getSkinInfo();
			if (skindatap &&
				skindatap->mJointKeys.size() > JOINT_COUNT_REQUIRED_FOR_FULLRIG &&
				!skindatap->mAlternateBindMatrix.empty())
			{
				mesh_id = skindatap->mMeshID;
				return true;
			}
		}
	}

	return false;
}

bool LLVOAvatar::jointIsRiggedTo(U32 joint_key)
{
	// Note: joint key 0 = "unnamed", 1 = "mScreen" (so we skip them)
	return joint_key > 1 && joint_key - 2 < mJointRiggingInfoTab.size() &&
		   mJointRiggingInfoTab[joint_key - 2].isRiggedTo();
}

void LLVOAvatar::clearAttachmentOverrides()
{
	// Note: joint key 0 = "unnamed", 1 = "mScreen" (so we skip them)
	for (U32 i = 2; i <= LL_CHARACTER_MAX_ANIMATED_JOINTS; ++i)
	{
		LLJoint* jointp = getJoint(i);
		if (jointp)
		{
			jointp->clearAttachmentPosOverrides();
			jointp->clearAttachmentScaleOverrides();
		}
	}

	if (mPelvisFixups.count())
	{
		mPelvisFixups.clear();
		if (mPelvisp)
		{
			mPelvisp->setPosition(LLVector3::zero);
		}
		postPelvisSetRecalc();
	}

	mActiveOverrideMeshes.clear();
	mJointRiggingInfoTab.setNeedsUpdate();
}

void LLVOAvatar::rebuildAttachmentOverrides()
{
	clearAttachmentOverrides();
	// Handle the case that we are resetting the skeleton of an animated object
	if (isPuppetAvatar())
	{
		LLVOVolume* volp = ((LLVOAvatarPuppet*)this)->mRootVolp;
		if (volp)
		{
			addAttachmentOverridesForObject(volp);
		}
	}

	// Attached objects
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* object = mAttachedObjectsVector[i].first;
		// Attached animated objects affect joints in their puppet, not the
		// avatar to which they are attached.
		if (object && !object->isAnimatedObject())
		{
			addAttachmentOverridesForObject(object);
		}
	}
}

void LLVOAvatar::updateAttachmentOverrides()
{
	uuid_list_t meshes_seen;

	if (isPuppetAvatar())
	{
		LLVOVolume* volp = ((LLVOAvatarPuppet*)this)->mRootVolp;
		if (volp)
		{
			addAttachmentOverridesForObject(volp, &meshes_seen);
		}
	}

	// Attached objects
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* object = mAttachedObjectsVector[i].first;
		// Attached animated objects affect joints in their puppet, not the
		// avatar to which they are attached.
		if (object && !object->isAnimatedObject())
		{
			addAttachmentOverridesForObject(object, &meshes_seen);
		}
	}

	// Remove meshes that are no longer present on the skeleton

	// Use a copy since removeAttachmentOverrides() will change
	// mActiveOverrideMeshes
	uuid_list_t active_override_meshes = mActiveOverrideMeshes;
	for (uuid_list_t::iterator it = active_override_meshes.begin(),
							   end = active_override_meshes.end();
		 it != end; ++it)
	{
		const LLUUID& mesh_id = *it;
		if (!meshes_seen.count(mesh_id))
		{
			removeAttachmentOverridesForObject(mesh_id);
		}
	}
}

void LLVOAvatar::addAttachmentOverridesForObject(LLViewerObject* vo,
												 uuid_list_t* meshes_seen,
												 bool recursive)
{
	if (!vo) return;	// Paranoia

	LLVOAvatar* av = vo->getAvatar();
	if (av != this && vo->getAvatarAncestor() != this)
	{
		// This case is fairly common (on login and TPs, i.e. when not all
		// objects data has been received) and not critical at all. Changed to
		// a debug message to avoid log spam. HB
		LL_DEBUGS("Avatar") << "Called with invalid avatar" << LL_ENDL;
		return;
	}

	if (recursive)
	{
		// Process all children
		LLViewerObject::const_child_list_t& children = vo->getChildren();
		for (LLViewerObject::const_child_list_t::const_iterator
				it = children.begin(), end = children.end();
			 it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp)
			{
				addAttachmentOverridesForObject(childp, meshes_seen, true);
			}
		}
	}

	LLVOVolume* vobj = vo->asVolume();
	if (!vobj || !vobj->getVolume() || !vobj->isMesh() ||
		!gMeshRepo.meshRezEnabled() ||
		!vobj->getVolume()->isMeshAssetLoaded())
	{
		return;
	}

	const LLMeshSkinInfo* skindatap = vobj->getSkinInfo();
	if (!skindatap)
	{
		return;
	}

	const S32 bind_count = skindatap->mAlternateBindMatrix.size();
	if (bind_count <= 0)
	{
		return;
	}

	const S32 joint_count = skindatap->mJointKeys.size();
	if (joint_count != bind_count)
	{
		llwarns_once << getFullname(true)
					 << " is wearing an invalid rigged mesh. bind_count = "
					 << bind_count << " - joint_count = " << joint_count
					 << " - Ignoring joint overrides." << llendl;
		return;
	}
	if (joint_count < (S32)JOINT_COUNT_REQUIRED_FOR_FULLRIG)
	{
		return;
	}

	std::string avname = getFullname(true);
	const LLUUID& mesh_id = skindatap->mMeshID;
	if (meshes_seen)
	{
		meshes_seen->emplace(mesh_id);
	}
	if (mActiveOverrideMeshes.count(mesh_id))
	{
		LL_DEBUGS("Avatar") << "Skipping add attachment overrides (already loaded) for mesh "
							<< mesh_id << " to root object "
							<< vobj->getRootEdit()->getID() << ", on avatar: "
							<< avname << LL_ENDL;
		return;
	}

	bool pelvis_got_set = false;
	const F32 pelvis_z_offset = skindatap->mPelvisOffset;
	LLUUID curr_id;
	LLVector3 pos_before, pos_after;
	bool override_changed = false;
	for (S32 i = 0; i < joint_count; ++i)
	{
		U32 joint_key = skindatap->mJointKeys[i];
		LLJoint* jointp = getJoint(joint_key);
		if (jointp)
		{
			// Set the joint position
			const LLVector3& pos =
				skindatap->mAlternateBindMatrix[i].getTranslation();
			if (jointp->aboveJointPosThreshold(pos))
			{
				jointp->addAttachmentPosOverride(pos, mesh_id, avname,
												 &override_changed);
				if (override_changed && joint_key == LL_JOINT_KEY_PELVIS)
				{
					pelvis_got_set = true;
				}
				if (skindatap->mLockScaleIfJointPosition)
				{
					// Note that unlike positions, there is no threshold check
					// here, just a lock at the default value.
					jointp->addAttachmentScaleOverride(jointp->getDefaultScale(),
													   mesh_id, avname);
				}
			}
		}
	}
	if (pelvis_z_offset != 0.f)
	{
		F32 fixup_before, fixup_after;
		bool has_fixup_before = hasPelvisFixup(fixup_before);
		addPelvisFixup(pelvis_z_offset, mesh_id);
		hasPelvisFixup(fixup_after);
		if (!has_fixup_before || fixup_before != fixup_after)
		{
			pelvis_got_set = true;
		}
	}

	mActiveOverrideMeshes.emplace(mesh_id);
	mJointRiggingInfoTab.setNeedsUpdate();

	// Rebuild body data if we altered joints/pelvis
	if (pelvis_got_set)
	{
		postPelvisSetRecalc();
	}
}

void LLVOAvatar::removeAttachmentOverridesForObject(LLViewerObject* vo)
{
	LLVOAvatar* av = vo ? vo->getAvatar() : NULL;
	if (av != this)
	{
		llwarns << "Called with invalid avatar" << llendl;
		return;
	}

	// Process all children
	LLViewerObject::const_child_list_t& children = vo->getChildren();
	for (LLViewerObject::const_child_list_t::const_iterator
			it = children.begin(), end = children.end();
		 it != end; ++it)
	{
		LLViewerObject* childp = *it;
		if (childp)
		{
			removeAttachmentOverridesForObject(childp);
		}
	}

	// Process self.
	LLUUID mesh_id;
	if (getRiggedMeshID(vo ,mesh_id))
	{
		removeAttachmentOverridesForObject(mesh_id);
	}
}

void LLVOAvatar::removeAttachmentOverridesForObject(const LLUUID& mesh_id)
{
	std::string avname = getFullname(true);
	// Note: joint key 0 = "unnamed", 1 = "mScreen" (so we skip them)
	for (U32 i = 2; i <= LL_CHARACTER_MAX_ANIMATED_JOINTS; ++i)
	{
		LLJoint* jointp = getJoint(i);
		if (jointp)
		{
			// Reset joints except for pelvis
			jointp->removeAttachmentPosOverride(mesh_id, avname);
			jointp->removeAttachmentScaleOverride(mesh_id, avname);
		}
		if (jointp == mPelvisp)
		{
			removePelvisFixup(mesh_id);
			jointp->setPosition(LLVector3::zero);
		}
	}

	postPelvisSetRecalc();

	mActiveOverrideMeshes.erase(mesh_id);
	mJointRiggingInfoTab.setNeedsUpdate();
}

LLVector3 LLVOAvatar::getCharacterPosition()
{
	if (mDrawable.notNull())
	{
		return mDrawable->getPositionAgent();
	}
	else
	{
		return getPositionAgent();
	}
}

LLQuaternion LLVOAvatar::getCharacterRotation()
{
	return getRotation();
}

LLVector3 LLVOAvatar::getCharacterVelocity()
{
	return getVelocity() - mStepObjectVelocity;
}

LLVector3 LLVOAvatar::getCharacterAngularVelocity()
{
	return getAngularVelocity();
}

void LLVOAvatar::getGround(const LLVector3& in_pos_agent,
						   LLVector3& out_pos_agent, LLVector3& out_norm)
{
	const LLVector3d z_vec(0.f, 0.f, 1.f);
	LLVector3d p0_global, p1_global;

	if (isUIAvatar())
	{
		out_norm.set(z_vec);
		out_pos_agent = in_pos_agent;
		return;
	}

	p0_global = gAgent.getPosGlobalFromAgent(in_pos_agent) + z_vec;
	p1_global = gAgent.getPosGlobalFromAgent(in_pos_agent) - z_vec;
	LLViewerObject* obj;
	LLVector3d out_pos_global;
	gWorld.resolveStepHeightGlobal(this, p0_global, p1_global, out_pos_global,
								   out_norm, &obj);
	out_pos_agent = gAgent.getPosAgentFromGlobal(out_pos_global);
}

F32 LLVOAvatar::getTimeDilation()
{
	return mRegionp ? mRegionp->getTimeDilation() : 1.f;
}

F32 LLVOAvatar::getPixelArea() const
{
	return isUIAvatar() ? 100000.f : mPixelArea;
}

LLVector3d LLVOAvatar::getPosGlobalFromAgent(const LLVector3& position)
{
	return gAgent.getPosGlobalFromAgent(position);
}

LLVector3 LLVOAvatar::getPosAgentFromGlobal(const LLVector3d& position)
{
	return gAgent.getPosAgentFromGlobal(position);
}

//virtual
void LLVOAvatar::requestStopMotion(LLMotion* motion)
{
	// Only agent avatars should handle the stop motion notifications.
}

// Loads <skeleton> node from XML tree
//virtual
bool LLVOAvatar::loadSkeletonNode()
{
	if (!LLAvatarAppearance::loadSkeletonNode())
	{
		return false;
	}

	initAttachmentPoints();

	return true;
}

// Creates attachment points if needed, sets state based on avatar_lad.xml
void LLVOAvatar::initAttachmentPoints(bool ignore_hud_joints)
{
	// ATTACHMENTS
	for (LLAvatarXmlInfo::attachment_info_list_t::iterator
			iter = sAvatarXmlInfo->mAttachmentInfoList.begin(),
			end = sAvatarXmlInfo->mAttachmentInfoList.end();
		 iter != end; ++iter)
	{
		LLAvatarXmlInfo::LLAvatarAttachmentInfo* info = *iter;
		if (info->mIsHUDAttachment && (ignore_hud_joints || !isSelf()))
		{
			// do not process HUD joint for other avatars, or when performing a
			// skeleton reset.
			continue;
		}

		S32 attachment_id = info->mAttachmentID;
		if (attachment_id < 1 || attachment_id > 255)
		{
			llwarns << "Avatar: " << getFullname(true)
					<< " - Attachment point out of range [1-255]: "
					<< attachment_id << " on attachment point "
					<< info->mName << ", skipping." << llendl;
			continue;
		}

		LLJoint* parent_joint = getJoint(info->mJointKey);
		if (!parent_joint)
		{
			// If the intended location for attachment point is unavailable,
			// stick it in a default location.
			// NOTE: this should not happen, unless avatar_lad.xml is corrupt
			llwarns << "Avatar: " << getFullname(true)
					<< " - No parent joint by name " << info->mJointName
					<< " found for attachment point " << info->mName
					 << ", using pelvis as the default parent." << llendl;
			llassert(false);
			parent_joint = mPelvisp;
		}

		// Check if the attachment already exists, so that we can reload
		// avatars...
		bool newly_created = false;
		LLViewerJointAttachment* attachment = get_ptr_in_map(mAttachmentPoints,
														 	 attachment_id);
		if (!attachment)
		{
			attachment = new LLViewerJointAttachment();
			newly_created = true;
		}

		attachment->setName(info->mName);

		if (info->mHasPosition)
		{
			attachment->setOriginalPosition(info->mPosition);
			attachment->setDefaultPosition(info->mPosition);
		}

		if (info->mHasRotation)
		{
			LLQuaternion rotation;
			rotation.setEulerAngles(info->mRotationEuler.mV[VX] * DEG_TO_RAD,
									info->mRotationEuler.mV[VY] * DEG_TO_RAD,
									info->mRotationEuler.mV[VZ] * DEG_TO_RAD);
			attachment->setRotation(rotation);
		}

		S32 group = info->mGroup;
		if (group >= 0)
		{
			if (group < 0 || group >= 9)
			{
				llwarns << "Avatar: " << getFullname(true)
						<< " - Invalid group number (" << group
						<< ") for attachment point " << info->mName
						<< llendl;
				llassert(false);
			}
			else
			{
				attachment->setGroup(group);
			}
		}

		attachment->setPieSlice(info->mPieMenuSlice);
		attachment->setVisibleInFirstPerson(info->mVisibleFirstPerson);
		attachment->setIsHUDAttachment(info->mIsHUDAttachment);

		// an attachment can potentially be animated and needs a number.
		attachment->setJointNum(mNumBones + mCollisionVolumes.size() +
								attachment_id - 1);

		if (newly_created)
		{
			mAttachmentPoints[attachment_id] = attachment;

			// now add attachment joint
			parent_joint->addChild(attachment);
		}
	}
}

void LLVOAvatar::updateVisualParams()
{
	LL_DEBUGS("Avatar") << "Called for avatar: " << getFullname(true)
						<< LL_ENDL;

	setSex(getVisualParamWeight("male") > 0.5f ? SEX_MALE : SEX_FEMALE);

	LLCharacter::updateVisualParams();

	if (mLastSkeletonSerialNum != mSkeletonSerialNum)
	{
		computeBodySize();
		mLastSkeletonSerialNum = mSkeletonSerialNum;
		mRoot->updateWorldMatrixChildren();
	}

	dirtyMesh();
	updateHeadOffset();
}

void LLVOAvatar::setPixelAreaAndAngle()
{
	if (mDrawable.isNull())
	{
		return;
	}

	const LLVector4a* ext = mDrawable->getSpatialExtents();
	LLVector4a center;
	center.setAdd(ext[1], ext[0]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(ext[1], ext[0]);
	size.mul(0.5f);

	mImpostorPixelArea = LLPipeline::calcPixelArea(center, size,
												   gViewerCamera);

	F32 range = mDrawable->mDistanceWRTCamera;
	if (range < 0.001f)		// range == zero
	{
		mAppAngle = 180.f;
	}
	else
	{
		F32 radius = size.getLength3().getF32();
		mAppAngle = atan2f(radius, range) * RAD_TO_DEG;
	}

	// We always want to look good to ourselves
	if (isSelf())
	{
		// Note: used to be 512 / 16, but increased to take into account larger
		// (1024x1024) new bakes. HB
		constexpr F32 MIN_AREA = 1024.f / 16.f;
		mPixelArea = llmax(mPixelArea, MIN_AREA);
	}
}

bool LLVOAvatar::updateJointLODs()
{
	constexpr F32 MAX_PIXEL_AREA = 100000000.f;
	F32 lod_factor = sLODFactor * AVATAR_LOD_TWEAK_RANGE + 1.f -
					 AVATAR_LOD_TWEAK_RANGE;
	F32 avatar_num_min_factor = clamp_rescale(sLODFactor, 0.f, 1.f, .25f, .6f);
	F32 avatar_num_factor = clamp_rescale((F32)sNumVisibleAvatars, 8, 25, 1.f,
										  avatar_num_min_factor);
	F32 area_scale = 0.16f;

	if (isSelf())
	{
		if (gAgent.cameraCustomizeAvatar() || gAgent.cameraMouselook())
		{
			mAdjustedPixelArea = MAX_PIXEL_AREA;
		}
		else
		{
			mAdjustedPixelArea = mPixelArea * area_scale;
		}
	}
	else if (mIsDummy)
	{
		mAdjustedPixelArea = MAX_PIXEL_AREA;
	}
	else
	{
		// Reported avatar pixel area is dependent on avatar render load,
		// based on number of visible avatars
		mAdjustedPixelArea = (F32)mPixelArea * area_scale * lod_factor *
							 lod_factor * avatar_num_factor *
							 avatar_num_factor;
	}

	// Now select meshes to render based on adjusted pixel area
	LLAvatarJoint* root = mRoot->asAvatarJoint();
	bool res = false;
	if (root)
	{
		res = root->updateLOD(mAdjustedPixelArea, true);
	}
	if (res)
	{
		++sNumLODChangesThisFrame;
		dirtyMesh(2);
		return true;
	}

	return false;
}

LLDrawable* LLVOAvatar::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);

	U32 pool_type, render_type;
	if (isPuppetAvatar())
	{
		pool_type = LLDrawPool::POOL_PUPPET;
		render_type = LLPipeline::RENDER_TYPE_PUPPET;
	}
	else
	{
		pool_type = LLDrawPool::POOL_AVATAR;
		render_type = LLPipeline::RENDER_TYPE_AVATAR;
	}

	// Only a single face (one per avatar); this face will be splitted into
	// several if its vertex buffer is too long.
	mDrawable->setState(LLDrawable::ACTIVE);
	LLDrawPoolAvatar* poolp = (LLDrawPoolAvatar*)gPipeline.getPool(pool_type);
	mDrawable->addFace(poolp, NULL);
	mDrawable->setRenderType(render_type);

	mNumInitFaces = mDrawable->getNumFaces();

	dirtyMesh(2);
	return mDrawable;
}

void LLVOAvatar::updateGL()
{
	if (mMeshTexturesDirty)
	{
		updateMeshTextures();
		mMeshTexturesDirty = false;
	}
}

#if 0	// A NOP functions doing pointless tests and always returning true...
		// Replaced with an inlined return true; in llvoavatar.h
bool LLVOAvatar::updateGeometry(LLDrawable* drawable)
{
	U32 type = isPuppetAvatar() ? LLPipeline::RENDER_TYPE_PUPPET
								: LLPipeline::RENDER_TYPE_AVATAR;
 	if (!gPipeline.hasRenderType(type) || !mMeshValid)
	{
		return true;
	}

	if (!drawable)
	{
		llwarns << getFullname(true) << " got a NULL drawable !" << llendl;
		llassert(false);
	}

	return true;
}
#endif

void LLVOAvatar::updateSexDependentLayerSets(bool upload_bake)
{
	invalidateComposite(mBakedTextureDatas[BAKED_HEAD].mTexLayerSet,
						upload_bake);
	invalidateComposite(mBakedTextureDatas[BAKED_UPPER].mTexLayerSet,
						upload_bake);
	invalidateComposite(mBakedTextureDatas[BAKED_LOWER].mTexLayerSet,
						upload_bake);
}

void LLVOAvatar::dirtyMesh()
{
	dirtyMesh(1);
}

void LLVOAvatar::dirtyMesh(S32 priority)
{
	mDirtyMesh = llmax(mDirtyMesh, priority);
}

LLViewerJoint* LLVOAvatar::getViewerJoint(S32 idx)
{
	LLAvatarJoint* avjointp = mMeshLOD[idx];
	if (avjointp)
	{
		return avjointp->asViewerJoint();
	}
	return NULL;
}

void LLVOAvatar::hideHair()
{
	mMeshLOD[MESH_ID_HAIR]->setVisible(false, true);
}

void LLVOAvatar::hideSkirt()
{
	mMeshLOD[MESH_ID_SKIRT]->setVisible(false, true);
}

bool LLVOAvatar::setParent(LLViewerObject* parent)
{
	bool ret;

	if (parent)
	{
		ret = LLViewerObject::setParent(parent);
		if (ret)
		{
			if (isSelf())
			{
				LL_DEBUGS("AgentSit") << "Sitting agent on new parent" << LL_ENDL;
			}
			sitOnObject(parent);
		}
	}
	else
	{
		if (isSelf())
		{
			LL_DEBUGS("AgentSit") << "Unsitting agent (NULL parent)" << LL_ENDL;
		}
		getOffObject();
		ret = LLViewerObject::setParent(parent);
		if (isSelf())
		{
			gAgent.resetCamera();
		}
	}

	return ret;
}

void LLVOAvatar::addChild(LLViewerObject* childp)
{
	// Find the inventory item this object is associated with:
	childp->extractAttachmentItemID();

	LLViewerObject::addChild(childp);
	if (childp->mDrawable)
	{
		attachObject(childp);
	}
	else
	{
		mPendingAttachment.push_back(childp);
	}
	if (isSelf())
	{
		gAttachmentsListDirty = true;
		gAttachmentsTimer.reset();
	}
}

void LLVOAvatar::removeChild(LLViewerObject* childp)
{
	if (childp)
	{
		LLViewerObject::removeChild(childp);
		detachObject(childp);
		if (isSelf())
		{
			gAttachmentsListDirty = true;
			gAttachmentsTimer.reset();
		}
	}
}

LLViewerJointAttachment* LLVOAvatar::getTargetAttachmentPoint(LLViewerObject* vobj)
{
	S32 attachment_id = ATTACHMENT_ID_FROM_STATE(vobj->getAttachmentState());

	// This should never happen unless the server did not process the
	// attachment point correctly, but putting this check in here to be safe.
	if (attachment_id & ATTACHMENT_ADD)
	{
		llwarns << "Got an attachment with ATTACHMENT_ADD mask. Removing mask (attach pt: "
				<< attachment_id << ")" << llendl;
		attachment_id &= ~ATTACHMENT_ADD;
	}

	LLViewerJointAttachment* attachment = get_ptr_in_map(mAttachmentPoints,
														 attachment_id);
	if (!attachment)
	{
		llwarns_once << getFullname(true)
					 << " is using invalid attachment point " << attachment_id
					 << llendl;
		// Arbitrary using 1 (chest)
		attachment = get_ptr_in_map(mAttachmentPoints, 1);
	}

	return attachment;
}

const LLViewerJointAttachment* LLVOAvatar::attachObject(LLViewerObject* vobj)
{
	LLViewerJointAttachment* attachment = getTargetAttachmentPoint(vobj);
	if (!attachment || !attachment->addObject(vobj, isSelf()))
	{
		return NULL;
	}

	mVisualComplexityStale = true;

	if (vobj->isSelected())
	{
		gSelectMgr.updateSelectionCenter();
		gSelectMgr.updatePointAt();
	}

	// Add the new (vobj, attachment) pair to the vector if not already there
	// (i.e. if not being reattached)
	attachments_vec_t::iterator end = mAttachedObjectsVector.end();
	std::pair<LLViewerObject*, LLViewerJointAttachment*> val(vobj, attachment);
	if (std::find(mAttachedObjectsVector.begin(), end, val) == end)
	{
		mAttachedObjectsVector.emplace_back(val);
	}

	if (!vobj->isAnimatedObject())
	{
		updateAttachmentOverrides();

		vobj->refreshBakeTexture();
		LLViewerObject::const_child_list_t& child_list = vobj->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp)
			{
				childp->refreshBakeTexture();
			}
		}
		updateMeshVisibility();
	}

	return attachment;
}

U32 LLVOAvatar::getNumAttachments() const
{
#if 0
	U32 num_attachments = 0;
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(),
										  end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		const LLViewerJointAttachment* attachment_pt = iter->second;
		if (attachment_pt)
		{
			num_attachments += attachment_pt->getNumObjects();
		}
		else
		{
			llwarns << "NULL joint attachment found for "
					<< ((LLVOAvatar*)this)->getFullname(true) << llendl;
		}
	}
	return num_attachments;
#else
	return mAttachedObjectsVector.size();
#endif
}

U32 LLVOAvatar::getNumAnimatedObjectAttachments() const
{
	U32 num_attachments = 0;
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(),
										  end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		const LLViewerJointAttachment* attachment_pt = iter->second;
		if (attachment_pt)
		{
			num_attachments += attachment_pt->getNumAnimatedObjects();
		}
		else
		{
			llwarns << "NULL joint attachment found for "
					<< ((LLVOAvatar*)this)->getFullname(true) << llendl;
		}
	}
	return num_attachments;
}

//virtual
S32 LLVOAvatar::getMaxAnimatedObjectAttachments() const
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || !regionp->getFeaturesReceived())
	{
		return 0;
	}

	const LLSD& info = regionp->getSimulatorFeatures();
	if (!info.has("AnimatedObjects"))
	{
		return 0;
	}

	return info["AnimatedObjects"]["MaxAgentAnimatedObjectAttachments"].asInteger();
}

bool LLVOAvatar::canAttachMoreAnimatedObjects(U32 n) const
{
	return getNumAnimatedObjectAttachments() + n <=
			(U32)getMaxAnimatedObjectAttachments();
}

void LLVOAvatar::lazyAttach()
{
	std::vector<LLPointer<LLViewerObject> > still_pending;

	for (U32 i = 0; i < mPendingAttachment.size(); ++i)
	{
		LLPointer<LLViewerObject> object = mPendingAttachment[i];
		if (object->isDead()) continue;

		const LLViewerJointAttachment* vja = NULL;
		if (object->mDrawable)
		{
			vja = attachObject(object);
		}
		if (vja)
		{
			if (isSelf())
			{
				LL_DEBUGS("Attachment") << "Attaching object "
										<< object->mID << " from "
										<< LLTrans::getString(vja->getName())
										<< LL_ENDL;
				gAttachmentsListDirty = true;
				gAttachmentsTimer.reset();
			}
		}
		else
		{
			still_pending.push_back(object);
		}
	}

	mPendingAttachment = still_pending;

	if (isSelf() && still_pending.size() > 0)
	{
		gAttachmentsListDirty = true;
		gAttachmentsTimer.reset();
	}
}

void LLVOAvatar::rebuildRiggedAttachments()
{
	LL_DEBUGS("Avatar") << "Called for avatar: " << getFullname(true)
						<< LL_ENDL;
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		const LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (object && object->mDrawable.notNull())
		{
			gPipeline.markRebuild(object->mDrawable);
		}
	}
}

#if 0	// Would it be at all needed ???
void LLVOAvatar::cleanupAttachedMesh(LLViewerObject* vobj)
{
	LLUUID mesh_id;
	if (getRiggedMeshID(vobj, mesh_id))
	{
		// Need to handle the repositioning of the cam, updating rig data etc
		// during outfit editing. This handles the case where we detach a
		// replacement rig.
		if (gAgent.cameraCustomizeAvatar())
		{
			gAgent.unpauseAnimation();	// not yet implemented
			// Still want to refocus on head bone
			gAgent.changeCameraToCustomizeAvatar();
		}
	}
}
#endif

//virtual
bool LLVOAvatar::detachObject(LLViewerObject* vobj)
{
	if (!vobj) return false;

	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment && attachment->isObjectAttached(vobj))
		{
			if (isSelf())
			{
				LL_DEBUGS("Attachment") << "Detaching object "
										<< vobj->mID << " from "
										<< LLTrans::getString(attachment->getName())
										<< LL_ENDL;
			}
			bool animated = vobj->isAnimatedObject();
#if 0
			cleanupAttachedMesh(vobj);
#endif
			vector_replace_with_last(mAttachedObjectsVector,
									 std::make_pair(vobj, attachment));
			attachment->removeObject(vobj, isSelf());
			if (!animated)
			{
				updateAttachmentOverrides();

				vobj->refreshBakeTexture();
				LLViewerObject::const_child_list_t& child_list =
					vobj->getChildren();
				for (LLViewerObject::child_list_t::const_iterator
						iter = child_list.begin(), end = child_list.end();
					 iter != end; ++iter)
				{
					LLViewerObject* childp = *iter;
					if (childp)
					{
						childp->refreshBakeTexture();
					}
				}
				updateMeshVisibility();
			}

			mVisualComplexityStale = true;

			return true;
		}
	}

	std::vector<LLPointer<LLViewerObject> >::iterator iter;
	iter = std::find(mPendingAttachment.begin(), mPendingAttachment.end(),
					 vobj);
	if (iter != mPendingAttachment.end())
	{
		mPendingAttachment.erase(iter);
		return true;
	}

	return false;
}

void LLVOAvatar::sitOnObject(LLViewerObject* sit_object)
{
	if (mDrawable.isNull())
	{
		return;
	}

	if (isSelf())
	{
		// Might be first sit
		LLFirstUse::useSit();

		gAgent.notOnSatGround();
		gAgent.setFlying(false);
		gAgent.setThirdPersonHeadOffset(LLVector3::zero);

		// Interpolate to new camera position
		gAgent.startCameraAnimation();
		if (gSavedSettings.getBool("SitCameraFrontView") &&
			gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK &&
			!gAgent.mForceMouselook)
		{
			gSavedSettings.setBool("CameraFrontView", true);
		}

		// Make sure we are not trying to autopilot
		gAgentPilot.stopAutoPilot();

		gAgent.setupSitCamera();
		if (gAgent.mForceMouselook)
		{
			gAgent.changeCameraToMouselook();
		}
	}

	LLQuaternion inv_obj_rot = ~sit_object->getRenderRotation();
	LLVector3 obj_pos = sit_object->getRenderPosition();

	LLVector3 rel_pos = getRenderPosition() - obj_pos;
	rel_pos.rotVec(inv_obj_rot);

	mDrawable->mXform.setPosition(rel_pos);
	mDrawable->mXform.setRotation(mDrawable->getWorldRotation() * inv_obj_rot);

	gPipeline.markMoved(mDrawable, true);
//MK
	if (gRLenabled && isSelf())
	{
		const LLUUID& obj_id = sit_object->getID();
		gRLInterface.setSitTargetId(obj_id);
		gRLInterface.notify("sat object legally", obj_id.asString());
	}
//mk
	mIsSitting = true;
	mRoot->getXform()->setParent(&sit_object->mDrawable->mXform);
	mRoot->setPosition(getPosition());
	mRoot->updateWorldMatrixChildren();

	stopMotion(ANIM_AGENT_BODY_NOISE);

	if (isSelf())
	{
		// *HACK: Disabling flying mode. This happens when we sat on an object
		// at a high altitude that was a few meters away from where the avatar
		// was standing. See VWR-16986 and VWR-19724.
		gAgent.setFlying(false);
	}
}

void LLVOAvatar::getOffObject()
{
	mIsSitting = false;

	if (mDrawable.isNull())
	{
		return;
	}

//MK
	bool force_re_sit = false;
//mk
	LLViewerObject* sit_object = (LLViewerObject*)getParent();
	if (sit_object)
	{
//MK
		if (gRLenabled && isSelf())
		{
			const LLUUID& obj_id = sit_object->getID();
			force_re_sit = gRLInterface.mContainsUnsit;
			if (force_re_sit)
			{
				gRLInterface.notify("unsat object illegally",
									obj_id.asString());
			}
			else
			{
				gRLInterface.setSitTargetId(LLUUID::null);
				gRLInterface.notify("unsat object legally",
									obj_id.asString());
			}
		}
//mk
		stopMotionFromSource(sit_object->getID());
		LLFollowCamMgr::setCameraActive(sit_object->getID(), false);

		LLViewerObject::const_child_list_t& child_list =
			sit_object->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child_objectp = *iter;

			stopMotionFromSource(child_objectp->getID());
			LLFollowCamMgr::setCameraActive(child_objectp->getID(), false);
		}
		if (isSelf() && !sit_object->permYouOwner() &&
//MK
			!force_re_sit &&
//mk
			gSavedSettings.getBool("RevokePermsOnStandUp"))
		{
			// First, revoke the animating permissions alone, then attempt to
			// revoke all other permissions: we must do that because for now,
			// in SL, the server trashes the whole message when trying to
			// revoke permissions other than animations-related ones.
			// Proceeding this way, let's a chance for all perms to actually be
			// revoked in OpenSim, and perhaps at a later date in SL...
			U32 permissions = LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TRIGGER_ANIMATION] |
							  LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS];
			gAgent.sendRevokePermissions(sit_object->getID(), permissions);
			permissions = 0xFFFFFFFF;
			gAgent.sendRevokePermissions(sit_object->getID(), permissions);
		}
	}

	// Assumes that transform will not be updated with drawable still having
	// a parent or that drawable had no parent from the start
	LLVector3 cur_position_world = mDrawable->getWorldPosition();
	LLQuaternion cur_rotation_world = mDrawable->getWorldRotation();

	// set *local* position based on last *world* position, since we're
	// unparenting the avatar
	mDrawable->mXform.setPosition(cur_position_world);
	mDrawable->mXform.setRotation(cur_rotation_world);

	gPipeline.markMoved(mDrawable, true);

	mRoot->getXform()->setParent(NULL);
	mRoot->setPosition(cur_position_world);
	mRoot->setRotation(cur_rotation_world);
	mRoot->getXform()->update();

	if (mEnableDefaultMotions)
	{
		startMotion(ANIM_AGENT_BODY_NOISE);
	}

	if (isSelf())
	{
 		LLQuaternion av_rot = gAgent.getFrameAgent().getQuaternion();
		LLQuaternion obj_rot = sit_object ? sit_object->getRenderRotation()
										  : LLQuaternion::DEFAULT;
		av_rot = av_rot * obj_rot;
		LLVector3 at_axis = LLVector3::x_axis;
		at_axis = at_axis * av_rot;
		at_axis.mV[VZ] = 0.f;
		at_axis.normalize();
		gAgent.resetAxes(at_axis);
#if 0
		// Reset orientation
		mRoot->setRotation(avWorldRot);
#endif
		gAgent.setThirdPersonHeadOffset(LLVector3(0.f, 0.f, 1.f));

		if (gSavedSettings.getBool("SitCameraFrontView"))
		{
			gSavedSettings.setBool("CameraFrontView", false);
		}

		gAgent.notOnSatGround();
		gAgent.setSitCamera(LLUUID::null);
//MK
#if 0	// Disabled, because this causes a race condition when executing:
		// @sit:<uuid>=force,unsit=n while the avatar is already sitting.

		// If we were sitting and prevented from standing up, and we are here,
		// the we probably received a message from the sim after a call to
		// llUnSit() in a LSL script. While we cannot ignore the sim message,
		// we still can force the avatar back down onto the seat.
		if (force_re_sit && sit_object->getRegion())
		{
			LL_DEBUGS("RestrainedLove") << "Forcing agent to re-sit on object"
										<< LL_ENDL;
			LL_DEBUGS("AgentSit") << "RestrainedLove sending agent force-re-sit on object request"
								  << LL_ENDL;
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_AgentRequestSit);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_TargetObject);
			msg->addUUIDFast(_PREHASH_TargetID, sit_object->mID);
# if 0		// Note: for seats without a sit target, transmitting the offset
			// results in a sit failure with "There is no suitable surface to
			// sit on" message, while transmitting a 0 offset seems to work, as
			// long as the seat is close to the avatar (8 meters away at most).
			msg->addVector3Fast(_PREHASH_Offset,
								gAgent.calcFocusOffset(sit_object,
													   gAgent.getPositionAgent(),
													   0, 0));
# else
			msg->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
# endif
			sit_object->getRegion()->sendReliableMessage();
		}
#endif
//mk
	}
}

//static
LLVOAvatar* LLVOAvatar::findAvatarFromAttachment(LLViewerObject* obj)
{
	if (obj->isAttachment())
	{
		do
		{
			obj = (LLViewerObject*)obj->getParent();
		}
		while (obj && !obj->isAvatar());

		if (obj && !obj->isDead())
		{
			return (LLVOAvatar*)obj;
		}
	}
	return NULL;
}

// Unlike most wearable functions, this works for both self and other.
bool LLVOAvatar::isWearingWearableType(LLWearableType::EType type) const
{
	if (mIsDummy) return true;

	if (isSelf())
	{
		return LLAvatarAppearance::isWearingWearableType(type);
	}

	switch (type)
	{
		case LLWearableType::WT_SHAPE:
		case LLWearableType::WT_SKIN:
		case LLWearableType::WT_HAIR:
		case LLWearableType::WT_EYES:
			return true;  // everyone has all bodyparts

		default:
			break; // Do nothing
	}

	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			tex_iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 tex_iter != end; ++tex_iter)
	{
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			tex_iter->second;
		if (t_dict->mWearableType == type)
		{
			// You are checking another avatar's clothing and you do not have
			// component textures. Thus, you must check to see if the
			// corresponding baked texture is defined.
			// NOTE: this is a poor substitute if you actually want to know
			// about individual pieces of clothing this works for detecting a
			// skirt (most important), but is ineffective at any piece of
			// clothing that gets baked into a texture that always exists
			// (upper or lower).
			if (t_dict->mIsUsedByBakedTexture)
			{
				const EBakedTextureIndex idx = t_dict->mBakedTextureIndex;
				return isTextureDefined(gAvatarAppDictp->getBakedTexture(idx)->mTextureIndex);
			}
			return false;
		}
	}
	return false;
}

//virtual
void LLVOAvatar::onGlobalColorChanged(const LLTexGlobalColor* global_color,
									  bool upload_bake)
{
	if (global_color == mTexSkinColor)
	{
		invalidateComposite(mBakedTextureDatas[BAKED_HEAD].mTexLayerSet,
							upload_bake);
		invalidateComposite(mBakedTextureDatas[BAKED_UPPER].mTexLayerSet,
							upload_bake);
		invalidateComposite(mBakedTextureDatas[BAKED_LOWER].mTexLayerSet,
							upload_bake);
	}
	else if (global_color == mTexHairColor)
	{
		invalidateComposite(mBakedTextureDatas[BAKED_HEAD].mTexLayerSet,
							upload_bake);
		invalidateComposite(mBakedTextureDatas[BAKED_HAIR].mTexLayerSet,
							upload_bake);

		// ! BACKWARDS COMPATIBILITY !
		// Fix for dealing with avatars from viewers that don't bake hair.
		if (!isTextureDefined(mBakedTextureDatas[BAKED_HAIR].mTextureIndex))
		{
			LLColor4 color = mTexHairColor->getColor();
			for (avatar_joint_mesh_list_t::iterator
					iter = mBakedTextureDatas[BAKED_HAIR].mJointMeshes.begin(),
					end = mBakedTextureDatas[BAKED_HAIR].mJointMeshes.end();
				 iter != end; ++iter)
			{
				LLAvatarJointMesh* mesh = *iter;
				if (mesh)
				{
					mesh->setColor(color);
				}
			}
		}
	}
	else if (global_color == mTexEyeColor)
	{
		invalidateComposite(mBakedTextureDatas[BAKED_EYES].mTexLayerSet,
							upload_bake);
	}
	updateMeshTextures();
}

bool LLVOAvatar::isVisible() const
{
	return mDrawable.notNull() && (!mOrphaned || isSelf()) &&
		   (mDrawable->isVisible() || isUIAvatar());
}

// Determine if we have enough avatar data to render
bool LLVOAvatar::getIsCloud()
{
	if (mIsDummy)
	{
		return false;
	}

	return visualParamWeightsAreDefault() ||
		   !isTextureDefined(TEX_LOWER_BAKED) ||
		   !isTextureDefined(TEX_UPPER_BAKED) ||
		   !isTextureDefined(TEX_HEAD_BAKED);
}

// Call periodically to keep isFullyLoaded up to date. Returns true if the
// value has changed.
bool LLVOAvatar::updateIsFullyLoaded()
{
	bool loading = getIsCloud();
	updateRuthTimer(loading);
	return processFullyLoadedChange(loading);
}

void LLVOAvatar::updateRuthTimer(bool loading)
{
 	if (isSelf() || !loading)
	{
		return;
	}

	if (!mPreviousFullyLoaded && sendAvatarTexturesRequest())
	{
		llinfos << "Ruth Timer timeout: Missing texture data for '"
				<< getFullname(true) << "' - Params loaded: "
				<< !visualParamWeightsAreDefault() << " - Lower: "
				<< isTextureDefined(TEX_LOWER_BAKED) << " - Upper: "
				<< isTextureDefined(TEX_UPPER_BAKED) << " - Head : "
				<< isTextureDefined(TEX_HEAD_BAKED) << llendl;
	}
}

bool LLVOAvatar::sendAvatarTexturesRequest(bool force)
{
	bool sent = false;
	if (force || mRuthTimer.getElapsedTimeF32() > DERUTHING_TIMEOUT_SECONDS)
	{
		std::vector<std::string> strings;
		strings.emplace_back(mID.asString());
		send_generic_message("avatartexturesrequest", strings);
		mRuthTimer.reset();
		sent = true;
	}
	return sent;
}

bool LLVOAvatar::processFullyLoadedChange(bool loading)
{
	// We wait a little bit before giving the all clear, to let textures settle
	// down
	constexpr F32 PAUSE = 1.f;
	if (loading)
	{
		mFullyLoadedTimer.reset();
	}

	mFullyLoaded = mFullyLoadedTimer.getElapsedTimeF32() > PAUSE;

	// Did our loading state "change" from last call ?
	constexpr S32 UPDATE_RATE = 30;
	bool fully_loaded_changed = mFullyLoaded != mPreviousFullyLoaded;
	// Changed...  If the value is different from the previous call,
	bool changed = fully_loaded_changed ||
				   // or if we have never been called before,
				   !mFullyLoadedInitialized ||
				   // or every now and then issue a change.
				   mFullyLoadedFrameCounter % UPDATE_RATE == 0;

	mPreviousFullyLoaded = mFullyLoaded;
	mFullyLoadedInitialized = true;
	++mFullyLoadedFrameCounter;

	if (changed && mFullyLoaded)
	{
		mVisualComplexityStale = true;
	}

	if (fully_loaded_changed)
	{
		LLPuppetMotion* motionp = getPuppetMotion();
		if (motionp)
		{
			motionp->updateSkeletonGeometry();
			gEventPumps.obtain("SkeletonUpdate").post(LLSD());
		}
	}

	return changed;
}

bool LLVOAvatar::isFullyLoaded(bool truly) const
{
	static LLCachedControl<bool> render_unloaded_avatar(gSavedSettings,
														"RenderUnloadedAvatar");
	return mFullyLoaded || (!truly && render_unloaded_avatar);
}

bool LLVOAvatar::isTooComplex() const
{
	static LLCachedControl<bool> always_friends(gSavedSettings,
												"AlwaysRenderFriends");
	static LLCachedControl<U32> max_cost(gSavedSettings,
										 "RenderAvatarMaxComplexity");
	static LLCachedControl<F32> max_area(gSavedSettings,
										 "RenderAutoMuteSurfaceAreaLimit");
	static LLCachedControl<U32> max_megabytes(gSavedSettings,
											  "RenderAutoMuteMemoryLimit");
	if (isSelf() || mVisuallyMuteSetting == AV_ALWAYS_RENDER ||
		(max_cost == 0 && max_area <= 0.f && max_megabytes == 0) ||
		(always_friends && LLAvatarTracker::isAgentFriend(mID)))
	{
		return false;
	}

	U32 max_bytes = max_megabytes * 1048576;
	return (max_cost > 0 && mVisualComplexity > max_cost) ||
		   (max_area > 0.f && mAttachmentSurfaceArea > max_area) ||
		   (max_bytes > 0 && mAttachmentGeometryBytes > max_bytes);
}

LLMotion* LLVOAvatar::findMotion(const LLUUID& id) const
{
	return mMotionController.findMotion(id);
}

void LLVOAvatar::debugColorizeSubMeshes(U32 i, const LLColor4& color)
{
	static LLCachedControl<bool> debug_avatar_composite_baked(gSavedSettings,
															  "DebugAvatarCompositeBaked");
	if (debug_avatar_composite_baked)
	{
		for (avatar_joint_mesh_list_t::iterator
				iter = mBakedTextureDatas[i].mJointMeshes.begin(),
				end = mBakedTextureDatas[i].mJointMeshes.end();
			 iter != end; ++iter)
		{
			LLAvatarJointMesh* mesh = *iter;
			if (mesh)
			{
				mesh->setColor(color);
			}
		}
	}
}

//virtual
void LLVOAvatar::updateMeshTextures()
{
	// If user has never specified a texture, assign the default
	for (U32 i = 0, count = getNumTEs(); i < count; ++i)
	{
		const LLViewerTexture* te_image = getImage(i, 0);
		if (!te_image || te_image->getID().isNull() ||
			te_image->getID() == IMG_DEFAULT)
		{
			// IMG_DEFAULT_AVATAR is a special texture that is never rendered.
			const LLUUID& image_id = i == TEX_HAIR ? IMG_DEFAULT
												   : IMG_DEFAULT_AVATAR;
			setImage(i,
					 LLViewerTextureManager::getFetchedTexture(image_id), 0);
		}
	}

	const bool other_culled = !isSelf() && mCulled;
	uuid_list_t* src_cb_list = NULL;
	bool paused = false;
	if (!isSelf())
	{
		src_cb_list = &mCallbackTextureList;
		paused = !isVisible();
	}

	U32 count = mBakedTextureDatas.size();
	std::vector<bool> is_layer_baked;
	is_layer_baked.resize(count, false);

	std::vector<bool> use_lkg_baked_layer; // lkg = "last known good"
	use_lkg_baked_layer.resize(count, false);

	for (U32 i = 0; i < count; ++i)
	{
		is_layer_baked[i] = isTextureDefined(mBakedTextureDatas[i].mTextureIndex);
		const LLUUID& last_tex_id = mBakedTextureDatas[i].mLastTextureID;
		if (other_culled)
		{
			use_lkg_baked_layer[i] = !is_layer_baked[i] &&
									 last_tex_id.notNull() &&
									 last_tex_id != IMG_DEFAULT_AVATAR;
			continue;
		}

		// When an avatar is changing clothes and not in Appearance mode, use
		// the last-known good baked texture until it finishes the first render
		// of the new layerset.
		LLViewerTexLayerSet* layerset = getTexLayerSet(i);
		bool layerset_invalid =
			layerset && (!layerset->isLocalTextureDataAvailable() ||
						 !layerset->getViewerComposite()->isInitialized());
		use_lkg_baked_layer[i] = !is_layer_baked[i] && layerset_invalid &&
								 last_tex_id.notNull() &&
								 last_tex_id != IMG_DEFAULT_AVATAR;
		if (use_lkg_baked_layer[i])
		{
			layerset->setUpdatesEnabled(true);
		}
	}

	bool local_appearance = isUsingLocalAppearance();
	LLViewerFetchedTexture* baked_img;
	for (U32 i = 0; i < count; ++i)
	{
		debugColorizeSubMeshes(i, LLColor4::white);

		LLViewerTexLayerSet* layerset = getTexLayerSet(i);
		if (use_lkg_baked_layer[i] && !local_appearance)
		{
			const LLUUID& last_tex_id = mBakedTextureDatas[i].mLastTextureID;
#if 0		// Causes failures to rebake...
			baked_img =
				LLViewerTextureManager::getFetchedTexture(last_tex_id);
#else
			ETextureIndex te = ETextureIndex(mBakedTextureDatas[i].mTextureIndex);
			std::string url = getImageURL(te, last_tex_id);
			if (url.empty())
			{
				// Baked textures should be requested from the sim this avatar
				// is on. JC
				const LLHost target_host = getObjectHost();
				if (!target_host.isOk())
				{
					llwarns << "invalid host for avatar: " << mID << llendl;
				}
				baked_img =
					LLViewerTextureManager::getFetchedTextureFromHost(last_tex_id,
																	  FTT_HOST_BAKE,
																	  target_host);
			}
			else
			{
				baked_img =
					LLViewerTextureManager::getFetchedTextureFromUrl(url,
																	 FTT_SERVER_BAKE,
																	 true,
																	 LLGLTexture::BOOST_NONE,
																	 LLViewerTexture::LOD_TEXTURE,
																	 0, 0,
																	 last_tex_id);
			}
#endif
			mBakedTextureDatas[i].mIsUsed = true;

			debugColorizeSubMeshes(i, LLColor4::red);

			for (avatar_joint_mesh_list_t::iterator
					iter = mBakedTextureDatas[i].mJointMeshes.begin(),
					end = mBakedTextureDatas[i].mJointMeshes.end();
				 iter != end; ++iter)
			{
				LLAvatarJointMesh* mesh = *iter;
				if (mesh)
				{
					mesh->setTexture(baked_img);
				}
			}
		}
		else if (!local_appearance && is_layer_baked[i])
		{
			baked_img =
				LLViewerTextureManager::staticCast(getImage(mBakedTextureDatas[i].mTextureIndex,
												  			0),
												   true);
			if (baked_img->getID() == mBakedTextureDatas[i].mLastTextureID)
			{
				// Even though the file may not be finished loading, we will
				// consider it loaded and use it (rather than doing
				// compositing).
				useBakedTexture(baked_img->getID());
			}
			else
			{
				mBakedTextureDatas[i].mIsLoaded = false;
				if (baked_img->getID() != IMG_INVISIBLE &&
					(i == BAKED_HEAD || i == BAKED_UPPER || i == BAKED_LOWER))
				{
					baked_img->setLoadedCallback(onBakedTextureMasksLoaded,
												 MORPH_MASK_REQUESTED_DISCARD,
												 true, true,
												 new LLTextureMaskData(mID),
												 src_cb_list, paused);
				}
				baked_img->setLoadedCallback(onBakedTextureLoaded,
											 SWITCH_TO_BAKED_DISCARD,
											 false, false, new LLUUID(mID),
											 src_cb_list, paused);
			}
		}
		else if (layerset && local_appearance)
		{
			debugColorizeSubMeshes(i, LLColor4::yellow);

			layerset->createComposite();
			layerset->setUpdatesEnabled(true);
			mBakedTextureDatas[i].mIsUsed = false;

			for (avatar_joint_mesh_list_t::iterator
					iter = mBakedTextureDatas[i].mJointMeshes.begin(),
					end = mBakedTextureDatas[i].mJointMeshes.end();
				 iter != end; ++iter)
			{
				LLAvatarJointMesh* mesh = *iter;
				if (mesh)
				{
					mesh->setLayerSet(layerset);
				}
			}
		}
		else
		{
			debugColorizeSubMeshes(i, LLColor4::blue);
		}
	}

	// Set texture and color of hair manually if we are not using a baked
	// image. This can happen while loading hair for yourself, or for clients
	// that did not bake a hair texture. Still needed for yourself after 1.22
	// is deprecated.
	if (!is_layer_baked[BAKED_HAIR] || isEditingAppearance())
	{
		const LLColor4 color = mTexHairColor ? mTexHairColor->getColor()
											 : LLColor4(1, 1, 1, 1);
		LLViewerTexture* hair_img = getImage(TEX_HAIR, 0);
		for (avatar_joint_mesh_list_t::iterator
				iter = mBakedTextureDatas[BAKED_HAIR].mJointMeshes.begin(),
				end = mBakedTextureDatas[BAKED_HAIR].mJointMeshes.end();
			 iter != end; ++iter)
		{
			LLAvatarJointMesh* mesh = *iter;
			if (mesh)
			{
				mesh->setColor(color);
				mesh->setTexture(hair_img);
			}
		}
	}

	if (isSelf())
	{
		for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator
				baked_iter = gAvatarAppDictp->getBakedTextures().begin(),
				end = gAvatarAppDictp->getBakedTextures().end();
			 baked_iter != end; ++baked_iter)
		{
			const EBakedTextureIndex baked_idx = baked_iter->first;
			const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
				baked_iter->second;

			for (texture_vec_t::const_iterator
					local_tex_iter = baked_dict->mLocalTextures.begin(),
					end2 = baked_dict->mLocalTextures.end();
				 local_tex_iter != end2; ++local_tex_iter)
			{
				const ETextureIndex texture_index = *local_tex_iter;
				bool is_baked_ready =
					is_layer_baked[baked_idx] &&
					mBakedTextureDatas[baked_idx].mIsLoaded;
				setBakedReady(texture_index, is_baked_ready);
			}
		}
	}

	// removeMissingBakedTextures() calls back into this routine when something
	// is removed, and would blow up the stack without this static flag trick.
	static bool call_remove_missing = true;
	if (call_remove_missing)
	{
		call_remove_missing = false;
		// May call back into this function if anything is removed:
		removeMissingBakedTextures();
		call_remove_missing = true;
	}
}

// Hides the mesh joints if attachments are using baked textures
void LLVOAvatar::updateMeshVisibility()
{
	bool bake_flag[BAKED_NUM_INDICES];
	memset(bake_flag, 0, BAKED_NUM_INDICES * sizeof(bool));

	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mAttachedObjectsVector[i].first;
		if (!objectp || objectp->isDead())
		{
			continue;
		}

		for (U8 te = 0, entries = objectp->getNumTEs(); te < entries; ++te)
		{
			LLTextureEntry* tep = objectp->getTE(te);
			if (!tep) continue;
			const LLUUID& tex_id = tep->getID();
			bake_flag[BAKED_HEAD] |= tex_id == IMG_USE_BAKED_HEAD;
			bake_flag[BAKED_UPPER] |= tex_id == IMG_USE_BAKED_UPPER;
			bake_flag[BAKED_LOWER] |= tex_id == IMG_USE_BAKED_LOWER;
			bake_flag[BAKED_HAIR] |= tex_id == IMG_USE_BAKED_HAIR;
			bake_flag[BAKED_EYES] |= tex_id == IMG_USE_BAKED_EYES;
			bake_flag[BAKED_SKIRT] |= tex_id == IMG_USE_BAKED_SKIRT;
			bake_flag[BAKED_LEFT_ARM] |= tex_id == IMG_USE_BAKED_LEFTARM;
			bake_flag[BAKED_LEFT_LEG] |= tex_id == IMG_USE_BAKED_LEFTLEG;
			bake_flag[BAKED_AUX1] |= tex_id == IMG_USE_BAKED_AUX1;
			bake_flag[BAKED_AUX2] |= tex_id == IMG_USE_BAKED_AUX2;
			bake_flag[BAKED_AUX3] |= tex_id == IMG_USE_BAKED_AUX3;
		}

		LLViewerObject::const_child_list_t& child_list =
			objectp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				it = child_list.begin(), end = child_list.end();
			it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (!childp || childp->isDead()) continue;

			for (U8 te = 0, entries = childp->getNumTEs(); te < entries; ++te)
			{
				LLTextureEntry* tep = childp->getTE(te);
				if (!tep) continue;
				const LLUUID& tex_id = tep->getID();
				bake_flag[BAKED_HEAD] |= tex_id == IMG_USE_BAKED_HEAD;
				bake_flag[BAKED_UPPER] |= tex_id == IMG_USE_BAKED_UPPER;
				bake_flag[BAKED_LOWER] |= tex_id == IMG_USE_BAKED_LOWER;
				bake_flag[BAKED_HAIR] |= tex_id == IMG_USE_BAKED_HAIR;
				bake_flag[BAKED_EYES] |= tex_id == IMG_USE_BAKED_EYES;
				bake_flag[BAKED_SKIRT] |= tex_id == IMG_USE_BAKED_SKIRT;
				bake_flag[BAKED_LEFT_ARM] |= tex_id == IMG_USE_BAKED_LEFTARM;
				bake_flag[BAKED_LEFT_LEG] |= tex_id == IMG_USE_BAKED_LEFTLEG;
				bake_flag[BAKED_AUX1] |= tex_id == IMG_USE_BAKED_AUX1;
				bake_flag[BAKED_AUX2] |= tex_id == IMG_USE_BAKED_AUX2;
				bake_flag[BAKED_AUX3] |= tex_id == IMG_USE_BAKED_AUX3;
			}
		}
	}

	LL_DEBUGS("Avatar") << "Baked mesh status for avatar "
					    << getFullname(true) << ":" << " head="
						<< bake_flag[BAKED_HEAD] << " - upper="
						<< bake_flag[BAKED_UPPER] << " - lower="
						<< bake_flag[BAKED_LOWER] << " - eyes="
						<< bake_flag[BAKED_EYES] << " - hair="
						<< bake_flag[BAKED_HAIR] << " - skirt="
						<< bake_flag[BAKED_LEFT_ARM] << " - leftarm="
						<< bake_flag[BAKED_LEFT_LEG] << " - leftleg="
						<< bake_flag[BAKED_AUX1] << " - aux1="
						<< bake_flag[BAKED_AUX2] << " - aux2="
						<< bake_flag[BAKED_AUX3] << " - aux3="
						<< bake_flag[BAKED_SKIRT] << LL_ENDL;

	for (S32 i = 0, count = mMeshLOD.size(); i < count; ++i)
	{
		LLAvatarJoint* joint = mMeshLOD[i];
		if (!joint) continue;

		if (i == MESH_ID_HEAD)
		{
			joint->setVisible(!bake_flag[BAKED_HEAD], true);
		}
		else if (i == MESH_ID_UPPER_BODY)
		{
			joint->setVisible(!bake_flag[BAKED_UPPER], true);
		}
		else if (i == MESH_ID_LOWER_BODY)
		{
			joint->setVisible(!bake_flag[BAKED_LOWER], true);
		}
		else if (i == MESH_ID_HAIR)
		{
			joint->setVisible(!bake_flag[BAKED_HAIR], true);
		}
		else if (i == MESH_ID_EYEBALL_LEFT)
		{
			joint->setVisible(!bake_flag[BAKED_EYES], true);
		}
		else if (i == MESH_ID_EYEBALL_RIGHT)
		{
			joint->setVisible(!bake_flag[BAKED_EYES], true);
		}
		else if (i == MESH_ID_EYELASH)
		{
			joint->setVisible(!bake_flag[BAKED_HEAD], true);
		}
		else if (i == MESH_ID_SKIRT)
		{
			joint->setVisible(!bake_flag[BAKED_SKIRT], true);
		}
	}
}

//virtual
void LLVOAvatar::setLocalTexture(ETextureIndex type, LLViewerTexture* in_tex,
								 bool baked_version_ready, U32 index)
{
	// Invalid for anyone but self
	llassert(false);
}

//virtual
void LLVOAvatar::setBakedReady(LLAvatarAppearanceDefines::ETextureIndex type,
							  bool baked_version_exists, U32 index)
{
	// Invalid for anyone but self
	llassert(false);
}

void LLVOAvatar::addChat(const LLChat& chat)
{
	std::deque<LLChat>::iterator chat_iter;

	mChats.emplace_back(chat);

	S32 chat_length = 0;
	for (chat_iter = mChats.begin(); chat_iter != mChats.end(); ++chat_iter)
	{
		chat_length += chat_iter->mText.size();
	}

	// Remove any excess chat
	chat_iter = mChats.begin();
	while ((chat_length > MAX_BUBBLE_CHAT_LENGTH ||
			(S32)mChats.size() > MAX_BUBBLE_CHAT_UTTERANCES) &&
		   chat_iter != mChats.end())
	{
		chat_length -= chat_iter->mText.size();
		mChats.pop_front();
		chat_iter = mChats.begin();
	}

	mChatTimer.reset();
}

void LLVOAvatar::clearChat()
{
	mChats.clear();
}

// Adds a morph mask to the appropriate baked texture structure
void LLVOAvatar::applyMorphMask(U8* tex_data, S32 width, S32 height,
								S32 num_components,
								LLAvatarAppearanceDefines::EBakedTextureIndex index)
{
	if (index >= BAKED_NUM_INDICES)
	{
		llwarns << "invalid baked texture index passed to applyMorphMask"
				<< llendl;
		return;
	}

	for (morph_list_t::const_iterator
			iter = mBakedTextureDatas[index].mMaskedMorphs.begin(),
			end = mBakedTextureDatas[index].mMaskedMorphs.end();
		 iter != end; ++iter)
	{
		const LLMaskedMorph* masked_morph = *iter;
		LLPolyMorphTarget* morph_target =
			masked_morph->mMorphTarget->asPolyMorphTarget();
		if (morph_target)
		{
			morph_target->applyMask(tex_data, width, height, num_components,
									masked_morph->mInvert);
		}
	}
}

#if 0	// not used...
// Returns true if morph masks are present and not valid for a given baked
// texture, false otherwise
bool LLVOAvatar::morphMaskNeedsUpdate(LLAvatarAppearanceDefines::EBakedTextureIndex index)
{
	if (index >= BAKED_NUM_INDICES)
	{
		return false;
	}

	if (!mBakedTextureDatas[index].mMaskedMorphs.empty())
	{
		if (isSelf())
		{
			LLViewerTexLayerSet* layer_set = getTexLayerSet(index);
			if (layer_set)
			{
				return !layer_set->isMorphValid();
			}
		}
		else
		{
			return false;
		}
	}

	return false;
}
#endif

// Releases any component texture UUIDs for which we have a baked texture
// ! BACKWARDS COMPATIBILITY !
// This is only called for non-self avatars, it can be taken out once component
// textures are not communicated by non-self avatars.
void LLVOAvatar::releaseComponentTextures()
{
	// ! BACKWARDS COMPATIBILITY !
	// Detect if the baked hair texture actually was not sent, and if so set to
	// default
	if (isTextureDefined(TEX_HAIR_BAKED) && getImage(TEX_HAIR_BAKED, 0) &&
		getImage(TEX_SKIRT_BAKED, 0) &&
		getImage(TEX_HAIR_BAKED, 0)->getID() == getImage(TEX_SKIRT_BAKED, 0)->getID())
	{
		if (getImage(TEX_HAIR_BAKED, 0)->getID() != IMG_INVISIBLE)
		{
			// Regression case of messaging system. Expected 21 textures,
			// received 20. Last texture is not valid so set to default.
			setTETexture(TEX_HAIR_BAKED, IMG_DEFAULT_AVATAR);
		}
	}

	bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);
	for (U8 baked_idx = 0; baked_idx < BAKED_NUM_INDICES; ++baked_idx)
	{
		const LLAvatarAppearanceDictionary::BakedEntry* baked_entry =
			gAvatarAppDictp->getBakedTexture((EBakedTextureIndex)baked_idx);
		// Skip if this is a skirt and av is not wearing one, or if we do not
		// have a baked texture UUID
		if ((baked_idx != BAKED_SKIRT || wearing_skirt) &&
			!isTextureDefined(baked_entry->mTextureIndex))
		{
			continue;
		}

		for (U8 texture = 0, count = baked_entry->mLocalTextures.size();
			 texture < count; ++texture)
		{
			const U8 te = (ETextureIndex)baked_entry->mLocalTextures[texture];
			setTETexture(te, IMG_DEFAULT_AVATAR);
		}
	}
}

void LLVOAvatar::dumpAvatarTEs(const std::string& context) const
{
	llinfos << (isSelf() ? "Self: " : "Other: ") << context << llendl;
	std::string message;
	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			iter->second;
		// *TODO: MULTI-WEARABLE: handle multiple textures for self
		const LLViewerTexture* te_image = getImage(iter->first, 0);
		if (!te_image)
		{
			message = "null pointer";
		}
		else if (te_image->getID().isNull())
		{
			message = "null UUID";
		}
		else if (te_image->getID() == IMG_DEFAULT)
		{
			message = "IMG_DEFAULT";
		}
		else if (te_image->getID() == IMG_INVISIBLE)
		{
			message = "IMG_INVISIBLE";
		}
		else if (te_image->getID() == IMG_DEFAULT_AVATAR)
		{
			message = "IMG_DEFAULT_AVATAR";
		}
		else
		{
			message = te_image->getID().asString();
		}
		llinfos << "       " << t_dict->mName << ": " << message << llendl;
	}
}

void LLVOAvatar::clampAttachmentPositions()
{
	if (isDead())
	{
		return;
	}
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment)
		{
			attachment->clampObjectPosition();
		}
	}
}

bool LLVOAvatar::hasHUDAttachment() const
{
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(),
										  end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment && attachment->getIsHUDAttachment() &&
			attachment->getNumObjects() > 0)
		{
			return true;
		}
	}
	return false;
}

LLBBox LLVOAvatar::getHUDBBox() const
{
	LLBBox bbox;

	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		const LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (object && object->isHUDAttachment())
		{
			// Initialize bounding box to contain identity orientation and
			// center point for attached object
			bbox.addPointLocal(object->getPosition());
			// add rotated bounding box for attached object
			bbox.addBBoxAgent(object->getBoundingBoxAgent());
			LLViewerObject::const_child_list_t& child_list =
				object->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				const LLViewerObject* child_objectp = *iter;
				bbox.addBBoxAgent(child_objectp->getBoundingBoxAgent());
			}
		}
	}

	return bbox;
}

void LLVOAvatar::onFirstTEMessageReceived()
{
	if (!mFirstTEMessageReceived)
	{
		mFirstTEMessageReceived = true;

		uuid_list_t* src_cb_list = NULL;
		bool paused = false;
		if (!isSelf())
		{
			src_cb_list = &mCallbackTextureList;
			paused = !isVisible();
		}

		for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
		{
			const bool layer_baked =
				isTextureDefined(mBakedTextureDatas[i].mTextureIndex);

			// Use any baked textures that we have even if they have not
			// downloaded yet (that is, do not do a transition from unbaked to
			// baked).
			if (layer_baked)
			{
				LLViewerFetchedTexture* image =
					LLViewerTextureManager::staticCast(getImage(mBakedTextureDatas[i].mTextureIndex,
																0),
													   true);
				mBakedTextureDatas[i].mLastTextureID = image->getID();
				// If we have more than one texture for the other baked layers,
				// we'll want to call this for them too.
				if (image->getID() != IMG_INVISIBLE &&
					(i == BAKED_HEAD || i == BAKED_UPPER || i == BAKED_LOWER))
				{
					image->setLoadedCallback(onBakedTextureMasksLoaded,
											 MORPH_MASK_REQUESTED_DISCARD,
											 true, true,
											 new LLTextureMaskData(mID),
											 src_cb_list, paused);
				}
				image->setLoadedCallback(onInitialBakedTextureLoaded,
										 MAX_DISCARD_LEVEL, false, false,
										 new LLUUID(mID), src_cb_list, paused);
			}
		}

		mMeshTexturesDirty = true;
		gPipeline.markGLRebuild(this);
		LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
								   << (intptr_t)this << std::dec << " ("
								   << getFullname(true) << ")" << LL_ENDL;
	}
}

bool LLVOAvatar::visualParamWeightsAreDefault()
{
	bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);

	for (LLVisualParam* param = getFirstVisualParam(); param;
	     param = getNextVisualParam())
	{
		if (param->isTweakable())
		{
			LLViewerVisualParam* vparam = param->asViewerVisualParam();
			bool is_skirt_param =
				vparam &&
				vparam->getWearableType() == LLWearableType::WT_SKIRT;
		    // We have to ignore whether skirt weights are default, if we are
			// not actually wearing a skirt
			if ((wearing_skirt || !is_skirt_param) &&
				param->getWeight() != param->getDefaultWeight())
			{
				return false;
			}
		}
	}

	return true;
}

struct LLAppearanceMessageContents : public LLRefCount
{
	LLAppearanceMessageContents()
	:	mAppearanceVersion(-1),
		mParamAppearanceVersion(-1),
		mCOFVersion(LLViewerInventoryCategory::VERSION_UNKNOWN)
	{
	}

	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variable, counting the 32 bits counter from LLRefCount. HB
	S32							mCOFVersion;
	S32							mAppearanceVersion;
	S32							mParamAppearanceVersion;
	std::vector<F32>			mParamWeights;
	std::vector<LLVisualParam*>	mParams;
	LLTEContents				mTEContents;
	LLVector3					mHoverOffset;
	bool						mHoverOffsetWasSet;
};

void LLVOAvatar::parseAppearanceMessage(LLMessageSystem* msg,
										LLAppearanceMessageContents& contents)
{
	parseTEMessage(msg, _PREHASH_ObjectData, -1, contents.mTEContents);

	// Parse the AppearanceData field, if any.
	if (msg->has(_PREHASH_AppearanceData))
	{
		U8 av_u8;
		msg->getU8Fast(_PREHASH_AppearanceData, _PREHASH_AppearanceVersion,
					   av_u8, 0);
		contents.mAppearanceVersion = av_u8;
		LL_DEBUGS("Avatar") << "Avatar: " << getFullname(true)
							<< " - appversion set by AppearanceData field: "
							<< contents.mAppearanceVersion << LL_ENDL;
		msg->getS32Fast(_PREHASH_AppearanceData, _PREHASH_CofVersion,
						contents.mCOFVersion, 0);
# if 0	// For future use:
		msg->getU32Fast(_PREHASH_AppearanceData, _PREHASH_Flags,
						appearance_flags, 0);
# endif
	}

	// Parse the AppearanceData field, if any.
	contents.mHoverOffsetWasSet = false;
	if (msg->has(_PREHASH_AppearanceHover))
	{
		LLVector3 hover;
		msg->getVector3Fast(_PREHASH_AppearanceHover, _PREHASH_HoverHeight,
						    hover);
		LL_DEBUGS("Avatar") << "Avatar: " << getFullname(true)
							<< " - hover received: " << hover.mV[VX] << ","
							<< hover.mV[VY] << "," << hover.mV[VZ] << LL_ENDL;
		contents.mHoverOffset = hover;
		contents.mHoverOffsetWasSet = true;
	}

	// Parse visual params, if any.
	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_VisualParam);
	if (num_blocks > 1)
	{
		LL_DEBUGS("Avatar") << getFullname(true)
							<< ": handle visual params, num_blocks "
							<< num_blocks << LL_ENDL;

		LLVisualParam* param = getFirstVisualParam();
		// if this ever fires, we should do the same as when num_blocks <= 1:
		llassert(param);
		if (!param)
		{
			llwarns << "No visual parameter !" << llendl;
		}
		else
		{
			for (S32 i = 0; i < num_blocks; ++i)
			{
				// Should not be any of group
				// VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT
				while (param &&
					   param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE)
				{
					param = getNextVisualParam();
				}

				if (!param)
				{
					// More visual params supplied than expected; just process
					// what we know about
					break;
				}

				U8 value;
				msg->getU8Fast(_PREHASH_VisualParam, _PREHASH_ParamValue,
							   value, i);
				F32 new_weight = U8_to_F32(value, param->getMinWeight(),
										  param->getMaxWeight());
				contents.mParamWeights.push_back(new_weight);
				contents.mParams.push_back(param);

				param = getNextVisualParam();
			}
		}
	}

	LLVisualParam* appearance_version_param = getVisualParam(11000);
	if (appearance_version_param)
	{
		std::vector<LLVisualParam*>::iterator it =
			std::find(contents.mParams.begin(), contents.mParams.end(),
					  appearance_version_param);
		if (it != contents.mParams.end())
		{
			S32 index = it - contents.mParams.begin();
			contents.mParamAppearanceVersion =
				ll_round(contents.mParamWeights[index]);
			LL_DEBUGS("Avatar") << "Index: " << index
								<< "appversion req by appearance_version param: "
								<< contents.mParamAppearanceVersion << LL_ENDL;
		}
	}
}

bool resolve_appearance_version(const LLAppearanceMessageContents& contents,
								S32& appearance_version)
{
	appearance_version = -1;

	if (contents.mAppearanceVersion >= 0 &&
		contents.mParamAppearanceVersion >= 0 &&
		contents.mAppearanceVersion != contents.mParamAppearanceVersion)
	{
		llwarns << "inconsistent appearance_version settings - field: "
				<< contents.mAppearanceVersion << ", param: "
				<< contents.mParamAppearanceVersion << llendl;
		return false;
	}
	if (contents.mParamAppearanceVersion >= 0) // use visual param if available.
	{
		appearance_version = contents.mParamAppearanceVersion;
	}
	if (contents.mAppearanceVersion >= 0)
	{
		appearance_version = contents.mAppearanceVersion;
	}
	if (appearance_version < 0) // still not set, go with 0.
	{
		appearance_version = 0;
	}
	LL_DEBUGS("Avatar") << "appearance version info - field "
						<< contents.mAppearanceVersion
						<< " param: " << contents.mParamAppearanceVersion
						<< " final: " << appearance_version << LL_ENDL;
	return true;
}

void LLVOAvatar::processAvatarAppearance(LLMessageSystem* msg)
{
	static LLCachedControl<bool> block_messages(gSavedSettings,
												"BlockAvatarAppearanceMessages");
	if (block_messages)
	{
		llwarns << "DEBUG MODE: Blocking AvatarAppearance message for: "
				<< getFullname(true) << llendl;
		return;
	}
	if (isSelf() && isEditingAppearance())
	{
		llinfos << "Ignoring appearance message while in appearance edit mode."
				<< llendl;
		return;
	}

	LL_DEBUGS("Avatar") << getFullname(true) << "("
						<< (isSelf() ? "self" : mID.asString())
						<< (!mFirstAppearanceMessageReceived ? ") - FIRST" : ") -")
						<< " AvatarAppearance message processing" << LL_ENDL;

	LLPointer<LLAppearanceMessageContents> contents(new LLAppearanceMessageContents);
	parseAppearanceMessage(msg, *contents);

	S32 num_params = contents->mParamWeights.size();
	if (num_params <= 1)
	{
		// In this case, we have no reliable basis for knowing appearance
		// version, which may cause us to look for baked textures in the wrong
		// place and flag them as missing assets.
		llinfos << "Ignoring appearance message due to lack of parameters"
				<< llendl;
		return;
	}

	S32 appearance_version;
	if (!resolve_appearance_version(*contents, appearance_version))
	{
		llwarns << "Bad appearance version info, discarding." << llendl;
		return;
	}
	S32 this_update_cof_version = contents->mCOFVersion;
	S32 last_update_request_cof_version = mLastUpdateRequestCOFVersion;

	// Only now that we have result of appearance_version can we decide whether
	// to bail out.
	if (isSelf())
	{
		LL_DEBUGS("Avatar") << "this_update_cof_version = "
							<< this_update_cof_version
							<< " - last_update_request_cof_version = "
							<< last_update_request_cof_version
							<< " - my_cof_version = "
							<< gAppearanceMgr.getCOFVersion() << LL_ENDL;

		if (!LLVOAvatarSelf::canUseServerBaking())
		{
			llwarns << "Received AvatarAppearance message for self in non-server-bake region"
					<< llendl;
		}
		if (mFirstTEMessageReceived && appearance_version == 0)
		{
			return;
		}

		// Check for stale update.
		if (appearance_version > 0 &&
			this_update_cof_version < last_update_request_cof_version)
		{
			llwarns << "Stale appearance update, wanted version "
					<< last_update_request_cof_version << ", got "
					<< this_update_cof_version << ". Ignoring." << llendl;
			return;
		}
	}

	// No backsies zone, if we get here, the message should be valid and usable
	if (appearance_version > 0)
	{
		mLastUpdateReceivedCOFVersion = this_update_cof_version;
	}
	setIsUsingServerBakes(appearance_version > 0);

	mLastProcessedAppearance = contents;
	applyParsedAppearanceMessage(*contents);
}

void LLVOAvatar::applyParsedAppearanceMessage(LLAppearanceMessageContents& contents,
											  bool slam_params)
{
	S32 num_params = contents.mParamWeights.size();
	ESex old_sex = getSex();

	if (applyParsedTEMessage(contents.mTEContents) > 0 && isChanged(TEXTURE))
	{
		mVisualComplexityStale = true;
	}

	// Prevent the overwriting of valid baked textures with invalid baked
	// textures
	for (U8 baked_idx = 0, count = mBakedTextureDatas.size();
		 baked_idx < count; ++baked_idx)
	{
		if (baked_idx != BAKED_SKIRT &&
			baked_idx != BAKED_LEFT_ARM && baked_idx != BAKED_LEFT_LEG &&
			baked_idx != BAKED_AUX1 && baked_idx != BAKED_AUX2 &&
			baked_idx != BAKED_AUX3 &&
			!isTextureDefined(mBakedTextureDatas[baked_idx].mTextureIndex) &&
			mBakedTextureDatas[baked_idx].mLastTextureID != IMG_DEFAULT)
		{
			setTEImage(mBakedTextureDatas[baked_idx].mTextureIndex,
				 	   LLViewerTextureManager::getFetchedTexture(mBakedTextureDatas[baked_idx].mLastTextureID,
																 FTT_DEFAULT,
																 true,
																 LLGLTexture::BOOST_NONE,
																 LLViewerTexture::LOD_TEXTURE));
		}
	}

	bool is_first_appearance_message = !mFirstAppearanceMessageReceived;
	mFirstAppearanceMessageReceived = true;

	// "Runway" fix was "if (!is_first_appearance_message)" which means it
	// would be called on second appearance message - Probably wrong.
	if (is_first_appearance_message)
	{
		onFirstTEMessageReceived();
	}

	setCompositeUpdatesEnabled(false);
	gPipeline.markGLRebuild(this);
	LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
							   << (intptr_t)this << std::dec << " ("
							   << getFullname(true) << ")" << LL_ENDL;

	// Apply visual params
	if (num_params > 1)
	{
		LL_DEBUGS("Avatar") << getFullname(true)
							<< ": handle visual params, num_blocks "
							<< num_params << LL_ENDL;
		bool params_changed = false;
		bool interp_params = false;

		for (S32 i = 0; i < num_params; ++i)
		{
			LLVisualParam* param = contents.mParams[i];
			if (!param) continue;	// Paranoia
			F32 new_weight = contents.mParamWeights[i];

			if (is_first_appearance_message ||
				param->getWeight() != new_weight)
			{
				params_changed = true;
				if (is_first_appearance_message || slam_params)
				{
					param->setWeight(new_weight, false);
				}
				else
				{
					interp_params = true;
					param->setAnimationTarget(new_weight, false);
				}
			}
		}

		// Do not worry about VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT
		const S32 expected_tweakable_count =
			getVisualParamCountInGroup(VISUAL_PARAM_GROUP_TWEAKABLE);
		if (num_params != expected_tweakable_count)
		{
			llwarns_once << getFullname(true)
						 << " - Number of params in AvatarAppearance msg ("
						 << num_params
						 << ") does not match number of tweakable params in avatar xml file ("
						 << expected_tweakable_count << "). Processing what we can."
						 << llendl;
		}

		if (params_changed)
		{
			if (interp_params)
			{
				startAppearanceAnimation();
			}
			updateVisualParams();

			ESex new_sex = getSex();
			if (old_sex != new_sex)
			{
				updateSexDependentLayerSets(false);
			}
		}
	}
	else
	{
		llwarns << getFullname(true)
				<< " - AvatarAppearance msg received without any visual parameter."
				<< llendl;

		// This is not really a problem if we already have a non-default shape
		if (visualParamWeightsAreDefault())
		{
			if (sendAvatarTexturesRequest())
			{
				// Re-requested appearance, hoping that it comes back with a
				// shape next time
				llinfos << "Re-requested AvatarAppearance for "
						<< getFullname(true) << llendl;
			}
		}
		else
		{
			// We do not really care.
			llinfos << "It is OK, we already have a non-default shape for "
					<< getFullname(true) << llendl;
		}
	}

	// Ignore hover updates for self because we have a more authoritative value
	// in the preferences.
	if (!isSelf())
	{
		if (contents.mHoverOffsetWasSet)
		{
			// Got an update for some other avatar.
			setHoverOffset(contents.mHoverOffset);
			LL_DEBUGS("Avatar") << "Avatar: " << getFullname(true)
								<< " - Setting hover from message: "
								<< contents.mHoverOffset.mV[VZ] << "m"
								<< LL_ENDL;
		}
		else
		{
			// If we do not get a value at all, we are presumably in a region
			// that does not support hover height.
			LL_DEBUGS("Avatar") << "Avatar: " << getFullname(true)
								<< " - Zeroing hover because not defined in appearance message"
								<< LL_ENDL;
			setHoverOffset(LLVector3::zero);
		}
	}

	setCompositeUpdatesEnabled(true);
	sAvatarCullingDirty = true;

	if (isSelf())
	{
		mUseLocalAppearance = false;
	}

	updateMeshTextures();
	refreshAttachmentBakes();
}

void LLVOAvatar::refreshAttachmentBakes()
{
	LL_DEBUGS("AttachmentBakes") << "Refreshing attachments bake textures for avatar "
								 << getFullname(true) << LL_ENDL;
	// Refresh bakes on any attached objects
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mAttachedObjectsVector[i].first;
		if (!objectp || objectp->isDead()) continue;

		objectp->refreshBakeTexture();

		LLViewerObject::const_child_list_t& child_list =
			objectp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				it = child_list.begin(), end = child_list.end();
			it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp || childp->isDead())
			{
				childp->refreshBakeTexture();
			}
		}
	}
	updateMeshVisibility();
}

//static
void LLVOAvatar::getAnimLabels(std::vector<std::string>* labels)
{
	labels->reserve(gUserAnimStatesCount);
	for (S32 i = 0; i < gUserAnimStatesCount; ++i)
	{
		labels->emplace_back(LLAnimStateLabels::getStateLabel(gUserAnimStates[i].mName));
	}

	// Special case to trigger away (AFK) state
	labels->emplace_back("Away From Keyboard");
}

//static
void LLVOAvatar::getAnimNames(std::vector<std::string>* names)
{
	names->reserve(gUserAnimStatesCount);
	for (S32 i = 0; i < gUserAnimStatesCount; ++i)
	{
		names->emplace_back(gUserAnimStates[i].mName);
	}

	// Special case to trigger away (AFK) state
	names->emplace_back("enter_away_from_keyboard_state");
}

void LLVOAvatar::onBakedTextureMasksLoaded(bool success,
										   LLViewerFetchedTexture* src_vi,
										   LLImageRaw* src,
										   LLImageRaw* aux_src,
										   S32 discard_level,
										   bool is_final,
										   void* userdata)
{
	if (!userdata) return;

	const LLUUID& id = src_vi->getID();

	LLTextureMaskData* maskData = (LLTextureMaskData*)userdata;
	LLVOAvatar* self = gObjectList.findAvatar(maskData->mAvatarID);

	// if discard level is 2 less than last discard level we processed, or we
	// hit 0, then generate morph masks
	if (self && success && (discard_level == 0 ||
							discard_level < maskData->mLastDiscardLevel - 2))
	{
		if (aux_src && aux_src->getComponents() == 1)
		{
			if (!aux_src->getData())
			{
				llerrs << "Missing auxiliary source data !" << llendl;
				return;
			}

			U32 gl_name;
			LLImageGL::generateTextures(1, &gl_name);
			LLTexUnit* unit = gGL.getTexUnit(0);
			unit->bindManual(LLTexUnit::TT_TEXTURE, gl_name);
			LLImageGL::setManualImage(GL_TEXTURE_2D, 0, GL_ALPHA8,
									  aux_src->getWidth(), aux_src->getHeight(),
									  GL_ALPHA, GL_UNSIGNED_BYTE,
									  aux_src->getData());
			unit->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);

			bool found_texture_id = false;
			for (LLAvatarAppearanceDictionary::Textures::const_iterator
					iter = gAvatarAppDictp->getTextures().begin(),
					end = gAvatarAppDictp->getTextures().end();
				 iter != end; ++iter)
			{
				const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
					iter->second;
				if (t_dict->mIsUsedByBakedTexture)
				{
					const ETextureIndex texture_index = iter->first;
					const LLViewerTexture* baked_img =
						self->getImage(texture_index, 0);
					if (baked_img && id == baked_img->getID())
					{
						const EBakedTextureIndex baked_idx =
							t_dict->mBakedTextureIndex;
						self->applyMorphMask(aux_src->getData(),
											 aux_src->getWidth(),
											 aux_src->getHeight(),
											 1,
											 baked_idx);
						maskData->mLastDiscardLevel = discard_level;
						if (self->mBakedTextureDatas[baked_idx].mMaskTexName)
						{
							LLImageGL::deleteTextures(1,
													  &(self->mBakedTextureDatas[baked_idx].mMaskTexName));
						}
						self->mBakedTextureDatas[baked_idx].mMaskTexName =
							gl_name;
						found_texture_id = true;
						break;
					}
				}
			}
			if (!found_texture_id)
			{
				llwarns_once << "Unexpected image id: " << id << llendl;
			}
			self->dirtyMesh();

			stop_glerror();
		}
		else
		{
            // This can happen when someone uses an old baked texture possibly
			// provided by viewer-side baked texture caching.
			// This is a very common and normal case, so let's make it a
			// llinfos instead of a llwarns...
			llinfos << "Masks loaded callback without aux source" << llendl;
		}
	}

	if (is_final || !success)
	{
		delete maskData;
	}
}

//static
void LLVOAvatar::onInitialBakedTextureLoaded(bool success,
											 LLViewerFetchedTexture* src_vi,
											 LLImageRaw* src,
											 LLImageRaw* aux_src,
											 S32 discard_level,
											 bool is_final,
											 void* userdata)
{
	LLUUID* avatar_idp = (LLUUID*)userdata;
	LLVOAvatar* selfp = gObjectList.findAvatar(*avatar_idp);

	if (!success && selfp)
	{
		selfp->removeMissingBakedTextures();
	}
	if (is_final || !success)
	{
		delete avatar_idp;
	}
}

void LLVOAvatar::onBakedTextureLoaded(bool success,
									  LLViewerFetchedTexture* src_vi,
									  LLImageRaw* src,
									  LLImageRaw* aux_src,
									  S32 discard_level,
									  bool is_final,
									  void* userdata)
{
	LLUUID id = src_vi->getID();
	LLUUID* avatar_idp = (LLUUID*)userdata;
	LLVOAvatar* selfp = gObjectList.findAvatar(*avatar_idp);

	if (selfp && !success)
	{
		selfp->removeMissingBakedTextures();
	}

	if (is_final || !success)
	{
		delete avatar_idp;
	}

	if (selfp && success && is_final)
	{
		selfp->useBakedTexture(id);
	}
}

// Called when baked texture is loaded and also when we start up with a baked
// texture
void LLVOAvatar::useBakedTexture(const LLUUID& id)
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		LLViewerTexture* image_baked =
			getImage(mBakedTextureDatas[i].mTextureIndex, 0);
		if (id == image_baked->getID())
		{
			mBakedTextureDatas[i].mIsLoaded = true;
			mBakedTextureDatas[i].mLastTextureID = id;
			mBakedTextureDatas[i].mIsUsed = true;

			if (isUsingLocalAppearance())
			{
				LL_DEBUGS("Avatar") << "Not changing to baked texture while using local appearance"
									<< LL_ENDL;
			}
			else
			{
				debugColorizeSubMeshes(i, LLColor4::green);

				for (avatar_joint_mesh_list_t::iterator
						iter = mBakedTextureDatas[i].mJointMeshes.begin(),
						end = mBakedTextureDatas[i].mJointMeshes.end();
					 iter != end; ++iter)
				{
					LLAvatarJointMesh* mesh = *iter;
					if (mesh)
					{
						mesh->setTexture(image_baked);
					}
				}
			}

			if (isSelf())
			{
				const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
					gAvatarAppDictp->getBakedTexture((EBakedTextureIndex)i);
				for (texture_vec_t::const_iterator
						local_tex_iter = baked_dict->mLocalTextures.begin(),
						local_tex_end = baked_dict->mLocalTextures.end();
					 local_tex_iter != local_tex_end; ++local_tex_iter)
				{
					setBakedReady(*local_tex_iter, true);
				}
			}

			// ! BACKWARDS COMPATIBILITY !
			// Workaround for viewing avatars from old viewers that do not have
			// baked hair textures. This is paired with similar code in
			// updateMeshTextures that sets hair mesh color.
			if (i == BAKED_HAIR)
			{
				for (avatar_joint_mesh_list_t::iterator
						iter = mBakedTextureDatas[i].mJointMeshes.begin(),
						end = mBakedTextureDatas[i].mJointMeshes.end();
					 iter != end; ++iter)
				{
					LLAvatarJointMesh* mesh = *iter;
					if (mesh)
					{
						mesh->setColor(LLColor4::white);
					}
				}
			}
		}
	}

	dirtyMesh();
}

void LLVOAvatar::getSortedJointNames(S32 joint_type,
									 std::vector<std::string>& result) const
{
	result.clear();
	if (joint_type == 0)
	{
		for (avatar_joint_list_t::const_iterator iter = mSkeleton.begin(),
												 end = mSkeleton.end();
			 iter != end; ++iter)
		{
			LLJoint* jointp = *iter;
			if (jointp)
			{
				result.emplace_back(jointp->getName());
			}
		}
	}
	else if (joint_type == 1)
	{
		for (S32 i = 0, count = mCollisionVolumes.size(); i < count; ++i)
		{
			LLAvatarJointCollisionVolume* jointp = mCollisionVolumes[i];
			if (jointp)
			{
				result.emplace_back(jointp->getName());
			}
		}
	}
	else if (joint_type == 2)
	{
		for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(),
											  end = mAttachmentPoints.end();
			 iter != end; ++iter)
		{
			LLViewerJointAttachment* jointp = iter->second;
			if (jointp)
			{
				result.emplace_back(jointp->getName());
			}
		}
	}

    std::sort(result.begin(), result.end());
}

void LLVOAvatar::dumpArchetypeXML(const std::string& filename)
{
	if (filename.empty()) return;

	LLFile outfile(filename, "wb");
	if (!outfile)
	{
		return;
	}

	fprintf(outfile,
			"<?xml version=\"1.0\" encoding=\"US-ASCII\" standalone=\"yes\"?>\n");
	fprintf(outfile, "<linden_genepool version=\"1.0\">\n");
	std::string name;
	gAgent.getName(name);
	fprintf(outfile, "\n\t<archetype name=\"%s\">\n", name.c_str());

	bool is_god = gAgent.isGodlikeWithoutAdminMenuFakery();
	std::string uuid_str;
	// Body parts and clothing.
	for (S32 type = LLWearableType::WT_SHAPE; type < LLWearableType::WT_COUNT;
		 ++type)
	{
		if (type > LLWearableType::WT_EYES && isSelf() &&
			!LLAgentWearables::selfHasWearable((LLWearableType::EType)type))
		{
			continue;
		}
		const std::string& wearable_name =
			LLWearableType::getTypeName((LLWearableType::EType)type);
		if (type <= LLWearableType::WT_EYES)
		{
			fprintf(outfile, "\n\t\t<!-- body part: %s -->\n",
					wearable_name.c_str());
		}
		else
		{
			fprintf(outfile, "\n\t\t<!-- wearable: %s -->\n",
					wearable_name.c_str());
		}

		for (LLVisualParam* param = getFirstVisualParam();
			 param; param = getNextVisualParam())
		{
			LLViewerVisualParam* viewer_param = (LLViewerVisualParam*)param;
			if (viewer_param->getWearableType() == type &&
				viewer_param->isTweakable())
			{
				fprintf(outfile,
						"\t\t<param id=\"%d\" name=\"%s\" value=\"%.3f\"/>\n",
						viewer_param->getID(),
						viewer_param->getName().c_str(),
						viewer_param->getWeight());
			}
		}

		for (U8 te = 0; te < TEX_NUM_INDICES; ++te)
		{
			if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == type)
			{
				// MULTIPLE_WEARABLES: extend to multiple wearables ?
				LLViewerTexture* te_image = getTEImage((ETextureIndex)te);
				if (!te_image)
				{
					continue;
				}
				const LLUUID& te_id = te_image->getID();
				if (is_god || HBObjectBackup::validateAssetPerms(te_id))
				{
					te_id.toString(uuid_str);
				}
				else
				{
					uuid_str = LLUUID::null.asString();
				}
				fprintf(outfile, "\t\t<texture te=\"%i\" uuid=\"%s\"/>\n", te,
						uuid_str.c_str());
			}
		}
	}

	// Root joint
	fprintf(outfile, "\n\t\t<!-- root -->\n");
	{
		const LLVector3& pos = mRoot->getPosition();
		const LLVector3& scale = mRoot->getScale();
		fprintf(outfile,
				"\t\t<root name=\"%s\" position=\"%f %f %f\" scale=\"%f %f %f\"/>\n",
				mRoot->getName().c_str(), pos[0], pos[1], pos[2], scale[0],
				scale[1], scale[2]);
	}

	// Bones
	fprintf(outfile, "\n\t\t<!-- bones -->\n");
	for (avatar_joint_list_t::iterator iter = mSkeleton.begin(),
									   end = mSkeleton.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (!jointp) continue;	// Paranoia

		const LLVector3& pos = jointp->getPosition();
		const LLVector3& scale = jointp->getScale();
		fprintf(outfile,
				"\t\t<bone name=\"%s\" position=\"%f %f %f\" scale=\"%f %f %f\"/>\n",
				jointp->getName().c_str(), pos[0], pos[1], pos[2],
				scale[0], scale[1], scale[2]);
	}

	// Collision volumes
	fprintf(outfile, "\n\t\t<!-- collision volumes -->\n");
	for (S32 i = 0, count = mCollisionVolumes.size(); i < count; ++i)
	{
		LLJoint* jointp = mCollisionVolumes[i];
		if (!jointp) continue;	// Paranoia

		const LLVector3& pos = jointp->getPosition();
		const LLVector3& scale = jointp->getScale();
		fprintf(outfile,
				"\t\t<collision_volume name=\"%s\" position=\"%f %f %f\" scale=\"%f %f %f\"/>\n",
				jointp->getName().c_str(), pos[0], pos[1], pos[2],
				scale[0], scale[1], scale[2]);
	}

	// Attachment joints
	fprintf(outfile, "\n\t\t<!-- attachments -->\n");
	for (attachment_map_t::iterator
			iter = mAttachmentPoints.begin(),
			end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* jointp = iter->second;
		if (!jointp) continue;

		const LLVector3& pos = jointp->getPosition();
		const LLVector3& scale = jointp->getScale();
		fprintf(outfile,
				"\t\t<attachment_point name=\"%s\" position=\"%f %f %f\" scale=\"%f %f %f\"/>\n",
				jointp->getName().c_str(), pos[0], pos[1], pos[2],
				scale[0], scale[1], scale[2]);
	}

	LLUUID mesh_id;
	bool got_some = false;
	std::set<LLVector3> distinct_overrides;
	// Joint pos overrides
	for (avatar_joint_list_t::iterator iter = mSkeleton.begin(),
									   end = mSkeleton.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (!jointp) continue;	// Paranoia

		LLVector3 pos;
		if (jointp->hasAttachmentPosOverride(pos, mesh_id))
		{
			if (!got_some)
			{
				got_some = true;
				fprintf(outfile, "\n\t\t<!-- joint position overrides -->\n");
			}
			distinct_overrides.clear();
			S32 n = jointp->getAllAttachmentPosOverrides(distinct_overrides);
			fprintf(outfile,
					"\t\t<joint_offset name=\"%s\" position=\"%f %f %f\" mesh_id=\"%s\" count=\"%d\" distinct=\"%d\"/>\n",
					jointp->getName().c_str(), pos[0], pos[1], pos[2],
					mesh_id.asString().c_str(), n,
					(S32)distinct_overrides.size());
		}
	}

	// Joint scale overrides
	got_some = false;
	LLVector3 scale;
	for (avatar_joint_list_t::iterator iter = mSkeleton.begin(),
									   end = mSkeleton.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (!jointp || !jointp->hasAttachmentScaleOverride(scale, mesh_id))
		{
			continue;
		}

		if (!got_some)
		{
			got_some = true;
			fprintf(outfile, "\n\t\t<!-- joint scale overrides -->\n");
		}

		distinct_overrides.clear();
		S32 n = jointp->getAllAttachmentScaleOverrides(distinct_overrides);
		fprintf(outfile,
				"\t\t<joint_scale name=\"%s\" scale=\"%f %f %f\" mesh_id=\"%s\" count=\"%d\" distinct=\"%d\"/>\n",
				jointp->getName().c_str(), scale[0], scale[1], scale[2],
				mesh_id.asString().c_str(), n, (S32)distinct_overrides.size());
	}

	F32 pelvis_fixup;
	if (hasPelvisFixup(pelvis_fixup, mesh_id))
	{
		fprintf(outfile,
				"\n\t\t<pelvis_fixup z=\"%f\" mesh_id=\"%s\"/>\n",
				pelvis_fixup, mesh_id.asString().c_str());
	}

	fprintf(outfile, "\t</archetype>\n");
	fprintf(outfile, "\n</linden_genepool>\n");
}

// Assumes LLVOAvatar::sInstances has already been sorted.
S32 LLVOAvatar::getUnbakedPixelAreaRank()
{
	S32 rank = 1;
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* inst = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (inst == this)
		{
			return rank;
		}
		else if (!inst->isDead() && !inst->isFullyBaked())
		{
			++rank;
		}
	}

	llassert(false);
	return 0;
}

struct CompareScreenAreaGreater
{
	bool operator()(const LLCharacter* const& lhs,
					const LLCharacter* const& rhs)
	{
		return lhs->getPixelArea() > rhs->getPixelArea();
	}
};

//static
void LLVOAvatar::cullAvatarsByPixelArea()
{
	LL_FAST_TIMER(FTM_CULL_AVATARS);

	sAvatarCullingDirty = false;

	std::sort(LLCharacter::sInstances.begin(), LLCharacter::sInstances.end(),
			  CompareScreenAreaGreater());

	// Update the avatars that have changed status
	bool has_non_baked_avatars = false;
	bool has_grey_avatars = false;
	U32 rank = 1;	// Avatar rank 0 is reserved for self
	U32 prank = 1;	// Animeshes have their own ranks.
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (!avatarp || avatarp->isDead())
		{
			continue;
		}

		bool culled = false;

		if (!avatarp->isFullyBaked())
		{
			has_non_baked_avatars = culled = true;
			has_grey_avatars |= avatarp->mHasGrey;
		}

		if (avatarp->isSelf())
		{
			// We never cull neither change the 0 visibility rank for self
			continue;
		}

		if (avatarp->mDrawable.isNull() || avatarp->mDrawable->isDead())
		{
			avatarp->mCulled = true;
			continue;
		}

		if (avatarp->mCulled != culled)
		{
			avatarp->mCulled = culled;
			LL_DEBUGS("Avatar") << "Avatar " << avatarp->getFullname(true)
								<< (culled ? " " : " not ") << "culled"
								<< LL_ENDL;
			avatarp->updateMeshTextures();
		}

		if (avatarp->mDrawable->isVisible())
		{
			avatarp->mVisibilityRank = avatarp->isPuppetAvatar() ? prank++
																 : rank++;
		}
	}

	if (has_non_baked_avatars)
	{
		// Update at most once per frame
		if (gFrameTimeSeconds != sUnbakedUpdateTime)
		{
			sUnbakedUpdateTime = gFrameTimeSeconds;
			sUnbakedTime += gFrameIntervalSeconds;
		}
		if (has_grey_avatars && gFrameTimeSeconds != sGreyUpdateTime)
		{
			sGreyUpdateTime = gFrameTimeSeconds;
			sGreyTime += gFrameIntervalSeconds;
		}
	}
}

void LLVOAvatar::startAppearanceAnimation()
{
	if (!mAppearanceAnimating)
	{
		mAppearanceAnimating = true;
		mAppearanceMorphTimer.reset();
		mLastAppearanceBlendTime = 0.f;
	}
}

//virtual
void LLVOAvatar::bodySizeChanged()
{
	if (isSelf())
	{
		// Notify simulator of change in size
		// NOTE: sendAgentSetAppearance() already checks to see if the
		// appearance is being modified and aborts in the latter case, so we
		// don't need to test for it here.
		gAgent.sendAgentSetAppearance();
	}
}

bool LLVOAvatar::isUsingServerBakes() const
{
#if 0
	// Sanity check - visual param for appearance version should match
	// mUseServerBakes
	LLVisualParam* appearance_version_param = getVisualParam(11000);
	llassert(appearance_version_param);
	F32 wt = appearance_version_param->getWeight();
	F32 expect_wt = mUseServerBakes ? 1.f : 0.f;
	if (!is_approx_equal(wt, expect_wt))
	{
		llwarns << "wt " << wt << " differs from expected " << expect_wt
				<< llendl;
	}
#endif
	return mUseServerBakes;
}

void LLVOAvatar::setIsUsingServerBakes(bool newval)
{
	mUseServerBakes = newval;
	LLVisualParam* appearance_version_param = getVisualParam(11000);
	llassert(appearance_version_param);
	appearance_version_param->setWeight(newval ? 1.f : 0.f, false);
}

std::string LLVOAvatar::getFullname(bool omit_resident)
{
	if (mLegacyName.empty())
	{
		LLNameValue* first = getNVPair("FirstName");
		if (first)
		{
			mLegacyName = first->getString();
		}
		LLNameValue* last = getNVPair("LastName");
		if (last)
		{
			std::string last_name = last->getString();
			if (!last_name.empty())
			{
				mNewResident = last_name == "Resident";
				if (!mNewResident)
				{
					mLegacyName += " ";
					mLegacyName += last_name;
				}
			}
		}
	}

	if (omit_resident || !mNewResident ||
		LLAvatarName::sOmitResidentAsLastName)
	{
		return mLegacyName;
	}
	else
	{
		return mLegacyName + " Resident";
	}
}

const LLHost& LLVOAvatar::getObjectHost() const
{
	if (isDead())
	{
		return LLHost::invalid;
	}
	LLViewerRegion* region = getRegion();
	return region ? region->getHost() : LLHost::invalid;
}

bool LLVOAvatar::updateLOD()
{
	if (mDrawable.isNull()) return false;

	if (isImpostor() && mDrawable->getNumFaces() != 0 &&
		mDrawable->getFace(0)->hasGeometry())
	{
		return true;
	}

	bool res = updateJointLODs();

	LLFace* facep = mDrawable->getFace(0);
	if (facep && !facep->getVertexBuffer())
	{
		dirtyMesh(2);
	}

	if (mDirtyMesh >= 2 || mDrawable->isState(LLDrawable::REBUILD_GEOMETRY))
	{
		// LOD changed or new mesh created, allocate new vertex buffer if
		// needed
		updateMeshData();
		mDirtyMesh = 0;
		mNeedsSkin = true;
		mDrawable->clearState(LLDrawable::REBUILD_GEOMETRY);
	}

	updateVisibility();

	return res;
}

void LLVOAvatar::updateLODRiggedAttachments()
{
	updateLOD();
	rebuildRiggedAttachments();
}

//virtual
void LLVOAvatar::updateRiggingInfo()
{
	mTempVolumes.clear();

	uuid_vec_t rigging_info_ids;
	std::vector<S32> rigging_info_lods;
	size_t rigs_count = mLastRiggingInfoLODs.size();
	rigging_info_ids.reserve(rigs_count);
	rigging_info_lods.reserve(rigs_count);

	// Will be set to true should we detect a change in the meshes or their LOD
	bool changed = false;

	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (!object || object->isHUDAttachment())
		{
			continue;
		}

		LLVOVolume* volp = object->asVolume();
		if (!volp)
		{
			continue;
		}
		mTempVolumes.push_back(volp);

		if (volp->isMesh() && volp->getVolume())
		{
			const LLUUID& mesh_id =
				volp->getVolume()->getParams().getSculptID();
			rigging_info_ids.emplace_back(mesh_id);
			S32 max_lod = llmax(volp->getLOD(), volp->mLastRiggingInfoLOD);
			rigging_info_lods.push_back(max_lod);
			if (!changed)
			{
				size_t i = rigging_info_lods.size() - 1;
				changed = i >= rigs_count ||
						  mLastRiggingInfoLODs[i] != max_lod ||
						  mLastRiggingInfoIDs[i] != mesh_id;
			}
		}

		if (volp->isAnimatedObject())
		{
			// For animated object attachments, we do not need the children. We
			// will just get bounding box from the puppet avatar.
			continue;
		}

		const_child_list_t& children = object->getChildren();
		for (const_child_list_t::const_iterator it = children.begin(),
												end = children.end();
			 it != end; ++it)
		{
			LLViewerObject* child = *it;
			if (!child)	// Paranoia
			{
				continue;
			}
			LLVOVolume* volp = child->asVolume();
			if (!volp)
			{
				continue;
			}
			mTempVolumes.push_back(volp);
			if (volp->isMesh() && volp->getVolume())
			{
				const LLUUID& mesh_id =
					volp->getVolume()->getParams().getSculptID();
				rigging_info_ids.emplace_back(mesh_id);
				S32 max_lod = llmax(volp->getLOD(), volp->mLastRiggingInfoLOD);
				rigging_info_lods.push_back(max_lod);
				if (!changed)
				{
					size_t i = rigging_info_lods.size() - 1;
					changed = i >= rigs_count ||
							  mLastRiggingInfoLODs[i] != max_lod ||
							  mLastRiggingInfoIDs[i] != mesh_id;
				}
			}
		}
	}

	if (isPuppetAvatar())
	{
		LLVOVolume* volp = ((LLVOAvatarPuppet*)this)->mRootVolp;
		if (volp)
		{
			mTempVolumes.push_back(volp);
			LLViewerObject::const_child_list_t& children = volp->getChildren();
			for (const_child_list_t::const_iterator it = children.begin(),
													end = children.end();
				 it != end; ++it)
			{
				LLViewerObject* child = *it;
				if (!child)	// Paranoia
				{
					continue;
				}
				volp = child->asVolume();
				if (!volp)
				{
					continue;
				}
				mTempVolumes.push_back(volp);
				if (volp->isMesh() && volp->getVolume())
				{
					const LLUUID& mesh_id =
						volp->getVolume()->getParams().getSculptID();
					rigging_info_ids.emplace_back(mesh_id);
					S32 max_lod = llmax(volp->getLOD(),
										volp->mLastRiggingInfoLOD);
					rigging_info_lods.push_back(max_lod);
					if (!changed)
					{
						size_t i = rigging_info_lods.size() - 1;
						changed = i >= rigs_count ||
								  mLastRiggingInfoLODs[i] != max_lod ||
								  mLastRiggingInfoIDs[i] != mesh_id;
					}
				}
			}
		}
	}

	// Check for key change, which indicates some change in volume composition
	// or LOD.
	if (changed)
	{
		mLastRiggingInfoIDs.swap(rigging_info_ids);
		mLastRiggingInfoLODs.swap(rigging_info_lods);
		mJointRiggingInfoTab.clear();
		for (S32 i = 0, count = mTempVolumes.size(); i < count; ++i)
		{
			LLVOVolume* volp = mTempVolumes[i];
			volp->updateRiggingInfo();
			mJointRiggingInfoTab.merge(volp->mJointRiggingInfoTab);
		}
	}
}

//virtual
U32 LLVOAvatar::getPartitionType() const
{
	// Avatars merely exist as drawables in the avatar partition
	return LLViewerRegion::PARTITION_AVATAR;
}

//static
void LLVOAvatar::updateImpostors()
{
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatar = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatar && !avatar->isDead() && avatar->isVisible() &&
			avatar->isImpostor() && avatar->needsImpostorUpdate())
		{
			gPipeline.generateImpostor(avatar);
		}
	}
}

//virtual
bool LLVOAvatar::isImpostor()
{
	return useImpostors() && mVisuallyMuteSetting != AV_ALWAYS_RENDER &&
		   (mUpdatePeriod >= IMPOSTOR_PERIOD || isVisuallyMuted());
}

void LLVOAvatar::cacheImpostorValues()
{
	getImpostorValues(mImpostorExtents, mImpostorAngle, mImpostorDistance);
}

void LLVOAvatar::getImpostorValues(LLVector4a* extents, LLVector3& angle,
								   F32& distance) const
{
	const LLVector4a* ext = mDrawable->getSpatialExtents();
	extents[0] = ext[0];
	extents[1] = ext[1];

	LLVector3 at = gViewerCamera.getOrigin() - getRenderPosition() -
				   mImpostorOffset;
	distance = at.normalize();
	F32 da = 1.f - at * gViewerCamera.getAtAxis();
	angle.mV[0] = gViewerCamera.getYaw() * da;
	angle.mV[1] = gViewerCamera.getPitch() * da;
	angle.mV[2] = da;
}

//static
void LLVOAvatar::updateSettings()
{
	static LLCachedControl<F32> lod_factor(gSavedSettings,
										   "RenderAvatarLODFactor");
	sLODFactor = llclamp((F32)lod_factor, 0.01f, 1.f);

	static LLCachedControl<F32> phys_lod(gSavedSettings,
										 "RenderAvatarPhysicsLODFactor");
	sPhysicsLODFactor = llclamp((F32)phys_lod, 0.f, 1.f);

	static LLCachedControl<S32> render_name(gSavedSettings, "RenderName");
	sRenderName = llclamp((S32)render_name, RENDER_NAME_NEVER,
						  RENDER_NAME_ALWAYS);

	static LLCachedControl<bool> hide_titles(gSavedSettings,
											 "RenderHideGroupTitleAll");
	sRenderGroupTitles = !hide_titles;

	static LLCachedControl<bool> self_visible(gSavedSettings,
											  "FirstPersonAvatarVisible");
	sVisibleInFirstPerson = self_visible;

	static LLCachedControl<U32> non_impostors(gSavedSettings,
											  "RenderAvatarMaxNonImpostors");
	sMaxNonImpostors = non_impostors;
//MK
	if (!gRLenabled || gRLInterface.mShowavsDistMax >= EXTREMUM)
//mk
	{
		sUseImpostors = sMaxNonImpostors != 0;
	}
//MK
	if (sMaxNonImpostors == 0)
	{
		// This is needed because RestrainedLove can force sUseImpostors to
		// true while the user configured RenderAvatarMaxNonImpostors to 0
		// (for "do not use impostors") and sMaxNonImpostors gets used if and
		// only if sUseImpostors is true (which would lead to impostoring all
		// avatars while the user didn't want any impostor at all).
		sMaxNonImpostors = 1000;	// No practical limit to non-impostors
	}
//mk
	LL_DEBUGS("Avatar") << "Use impostors: " << (sUseImpostors ? "yes" : "no")
						<< " - Max non-impostors: " << sMaxNonImpostors
						<< LL_ENDL;

	static LLCachedControl<U32> max_puppets(gSavedSettings,
											"RenderAvatarMaxPuppets");
	sMaxNonImpostorsPuppets = max_puppets;
	sUsePuppetImpostors = max_puppets != 0;
	LL_DEBUGS("Avatar") << "Use puppet impostors: "
						<< (sMaxNonImpostorsPuppets ? "yes" : "no")
						<< " - Max non-impostor puppets: "
						<< sMaxNonImpostorsPuppets << LL_ENDL;
}

void LLVOAvatar::idleUpdateRenderComplexity()
{
	if (isPuppetAvatar())
	{
		LLVOVolume* volp = ((LLVOAvatarPuppet*)this)->mRootVolp;
		if (volp && volp->isAttachment())
		{
			// Render cost for attached animated objects is accounted as any
			// other attachment.
			return;
		}
	}

    calculateUpdateRenderComplexity();	// Update mVisualComplexity if needed

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AVATAR_DRAW_INFO))
	{
		setDebugText(llformat("%d\nrank %d\nperiod %d", mVisualComplexity,
							  mVisibilityRank, mUpdatePeriod));

		static LLCachedControl<U32> max_cost(gSavedSettings,
											 "RenderAvatarMaxComplexity");
		if (max_cost)
		{
			F32 green = 1.f - llclamp(((F32)mVisualComplexity - (F32)max_cost) /
									  (F32)max_cost, 0.f, 1.f);
			F32 red = llmin((F32)mVisualComplexity / (F32)max_cost, 1.f);
			mText->setColor(LLColor4(red, green, 0.f, 1.f));
		}
	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_ATTACHMENT_INFO))
	{
		setDebugText(llformat("%.1f MB, %.2f m2",
							  (F32)mAttachmentGeometryBytes / 1048576.f,
							  mAttachmentSurfaceArea));

		static LLCachedControl<U32> max_megabytes(gSavedSettings,
												  "RenderAutoMuteMemoryLimit");
		static LLCachedControl<F32> max_area(gSavedSettings,
											 "RenderAutoMuteSurfaceAreaLimit");
		bool mem_limit = max_megabytes > 0;
		bool area_limit = max_area > 0.f;
		if (!mem_limit && !area_limit)
		{
			return;
		}

		F32 green = 0.f;
		F32 red = 1.f;
		F32 current_value = (F32)mAttachmentGeometryBytes / 1048576.f;
		if (!(mem_limit && current_value >= (F32)max_megabytes) &&
			!(area_limit && mAttachmentSurfaceArea >= (F32)max_area))
		{
			F32 max_value = 0.f;
			if (mem_limit)
			{
				max_value = (F32)max_megabytes;
			}
			else
			{
				current_value = 0.f;
			}
			if (area_limit)
			{
				max_value += max_area;
				current_value += mAttachmentSurfaceArea;
			}
			green = 1.f - llclamp((current_value - max_value) / max_value,
								  0.f, 1.f);
			red = llmin(current_value / max_value, 1.f);
		}
		mText->setColor(LLColor4(red, green, 0.f, 1.f));
	}
}

void LLVOAvatar::accountRenderComplexityForObject(LLViewerObject* object,
												  U32& cost)
{
	if (!object || object->isHUDAttachment())
	{
		return;
	}

	const LLDrawable* drawable = object->mDrawable;
	if (!drawable) return;

	const LLVOVolume* volume = drawable->getVOVolume();
	if (!volume) return;

	F32 attachment_total_cost = 0.f;
	F32 attachment_volume_cost = 0.f;
	F32 attachment_texture_cost = 0.f;
	F32 attachment_children_cost = 0.f;

	constexpr F32 animated_object_attachment_surcharge = 1000.f;
	if (object->isAnimatedObject())
	{
		attachment_volume_cost += animated_object_attachment_surcharge;
	}

	static LLVOVolume::texture_cost_t textures;
	textures.clear();
	attachment_volume_cost = volume->getRenderCost(textures);

	const_child_list_t& children = volume->getChildren();
	for (const_child_list_t::const_iterator
			child_iter = children.begin(), child_end = children.end();
		 child_iter != child_end; ++child_iter)
	{
		LLViewerObject* child_obj = *child_iter;
		if (!child_obj) continue;	// Paranoia

		LLVOVolume* child = child_obj->asVolume();
		if (child)
		{
			attachment_children_cost += child->getRenderCost(textures);
		}
	}

	for (LLVOVolume::texture_cost_t::iterator
			tex_iter = textures.begin(), tex_end = textures.end();
		 tex_iter != tex_end; ++tex_iter)
	{
		// Add the cost of each individual texture in the linkset
		attachment_texture_cost += tex_iter->second;
	}

	attachment_total_cost = attachment_volume_cost + attachment_texture_cost +
							attachment_children_cost;
	LL_DEBUGS("ARCdetail") << getFullname(true) << ", costs for attachment: "
						   << object-> getAttachmentItemID() << " - total: "
						   << attachment_total_cost << " - volumes: "
						   << attachment_volume_cost << " - textures: "
						   << attachment_texture_cost << " - "
						   << volume->numChildren() << " children: "
						   << attachment_children_cost << LL_ENDL;
	// Limit attachment complexity to avoid signed integer flipping of the
	// wearer's ACI
	cost += (U32)llclamp(attachment_total_cost, 0.f,
						 MAX_ATTACHMENT_COMPLEXITY);
}

// Calculations for mVisualComplexity value
void LLVOAvatar::calculateUpdateRenderComplexity()
{
	if (!mVisualComplexityStale ||
		gFrameTimeSeconds - mComplexityUpdateTime < COMPLEXITY_UPDATE_INTERVAL)
	{
		return;
	}

	/**************************************************************************
	 * This calculation should not be modified by third party viewers, since it
	 * is used to limit rendering and should be uniform for everyone. If you
	 * have suggested improvements, submit them to the official viewer for
	 * consideration.
	 *************************************************************************/
	constexpr U32 COMPLEXITY_BODY_PART_COST = 200;
	U32 cost = 0;

	bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);
	bool debug_alpha = LLDrawPoolAlpha::sShowDebugAlpha;
	for (U8 baked_idx = 0; baked_idx <= BAKED_HAIR; ++baked_idx)
	{
		const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
			gAvatarAppDictp->getBakedTexture((EBakedTextureIndex)baked_idx);
		ETextureIndex tex_index = baked_dict->mTextureIndex;
		if (tex_index == TEX_SKIRT_BAKED && !wearing_skirt)
		{
			continue;
		}
		// Same as logic as in isTextureVisible(), but does not account for
		// isSelf() so to ensure identical numbers for all avatars.
		if (isIndexLocalTexture(tex_index))
		{
			if (isTextureDefined(tex_index))
			{
				cost += COMPLEXITY_BODY_PART_COST;
			}
			continue;
		}
		// Baked textures can use TE images directly
		if (isTextureDefined(tex_index) &&
			(debug_alpha || getTEImage(tex_index)->getID() != IMG_INVISIBLE))
		{
			cost += COMPLEXITY_BODY_PART_COST;
		}
	}
	LL_DEBUGS("ARCdetail") << getFullname(true) << "'s body parts complexity: "
						   << cost << LL_ENDL;

	// A standalone animated object needs to be accounted for using its
	// associated volume. Attached animated objects will be covered by the
	// subsequent loop over attachments.
	if (isPuppetAvatar())
	{
		LLVOVolume* volp = ((LLVOAvatarPuppet*)this)->mRootVolp;
		if (volp && !volp->isAttachment())
		{
			accountRenderComplexityForObject((LLViewerObject*)volp, cost);
		}
	}

	// Account for complexity of all attachments.
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (!object->isTempAttachment())
		{
			accountRenderComplexityForObject(object, cost);
		}
	}

	static LLCachedControl<bool> show_changes(gSavedSettings,
											  "ShowMyComplexityChanges");
	if (isSelf() && show_changes && mVisualComplexity != cost)
	{
		static LLUUID last_notif_id;
		if (last_notif_id.notNull())
		{
			LLNotificationPtr n = gNotifications.find(last_notif_id);
			if (n)
			{
				gNotifications.cancel(n);
			}
			last_notif_id.setNull();
		}
		LLSD args;
		args["AGENT_COMPLEXITY"] = llformat("%d", cost);
		U32 attachments = getNumAttachments();
		args["[ATTACHMENTS]"] = llformat("%d", attachments);
		args["[SLOTS]"] = llformat("%d", gMaxSelfAttachments - attachments);
		LLNotificationPtr n = gNotifications.add("AgentComplexity", args);
		if (n)
		{
			last_notif_id = n->getID();
		}
	}

	mVisualComplexity = cost;
	mVisualComplexityStale = false;
	mComplexityUpdateTime = gFrameTimeSeconds;
}

void LLVOAvatar::setVisualMuteSettings(VisualMuteSettings value)
{
	if (!mCachedMute && !isUIAvatar())
	{
//MK
		if (gRLenabled && mCachedRLVMute && value == AV_ALWAYS_RENDER)
		{
			// Allow to switch from AV_DO_NOT_RENDER to normal rendering, but
			// do not let the user override the RLV mute with AV_ALWAYS_RENDER
			value = AV_RENDER_NORMALLY;
		}
//mk
		mVisuallyMuteSetting = value;
		mNeedsImpostorUpdate = true;
		mCachedVisualMuteUpdateTime = 0.f;
	}
}

//static
bool LLVOAvatar::isIndexLocalTexture(ETextureIndex index)
{
	if (index < 0 || index >= TEX_NUM_INDICES) return false;
	const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
		gAvatarAppDictp->getTexture(index);
	return t_dict && t_dict->mIsLocalTexture;
}

//static
bool LLVOAvatar::isIndexBakedTexture(ETextureIndex index)
{
	if (index < 0 || index >= TEX_NUM_INDICES) return false;
	const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
		gAvatarAppDictp->getTexture(index);
	return t_dict && t_dict->mIsBakedTexture;
}

const std::string LLVOAvatar::getBakedStatusForPrintout() const
{
	std::string line;

	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			iter->second;
		if (t_dict && t_dict->mIsBakedTexture &&
			t_dict->mBakedTextureIndex < gAgent.mUploadedBakes)
		{
			line += t_dict->mName;
			if (isTextureDefined(index))
			{
				line += "_baked";
			}
			line += " ";
		}
	}
	return line;
}

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

F32 calc_bouncy_animation(F32 x)
{
	return -(cosf(x * F_PI * 2.5f - F_PI_BY_TWO)) * (0.4f + x * -0.1f) +
		   x * 1.3f;
}

//virtual
bool LLVOAvatar::isTextureDefined(LLAvatarAppearanceDefines::ETextureIndex te,
								  U32 index) const
{
	if (isIndexLocalTexture(te))
	{
		return false;
	}

	LLViewerTexture* imagep = getImage(te, index);
	if (imagep)
	{
		const LLUUID& id = imagep->getID();
		return id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT;
	}

	llwarns << "getImage(" << te << ", " << index << ") returned NULL !"
			<< llendl;
	return false;
}

//virtual
bool LLVOAvatar::isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
								  U32 index) const
{
	if (isIndexLocalTexture(type))
	{
		return isTextureDefined(type, index);
	}

	// Baked textures can use TE images directly
	return ((isTextureDefined(type) || isSelf()) &&
		    (getTEImage(type)->getID() != IMG_INVISIBLE ||
			 LLDrawPoolAlpha::sShowDebugAlpha));
}

//virtual
bool LLVOAvatar::isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
								  LLViewerWearable* wearable) const
{
	// Non-self avatars do not have wearables
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLVOAvatarUI class
///////////////////////////////////////////////////////////////////////////////

LLVOAvatarUI::LLVOAvatarUI(const LLUUID& id, LLViewerRegion* regionp)
:	LLVOAvatar(id, regionp)
{
	mIsDummy = true;
}

//virtual
void LLVOAvatarUI::initInstance()
{
	LLVOAvatar::initInstance();

	createDrawable();
	setPositionAgent(LLVector3::zero);
	slamPosition();
	updateJointLODs();
	updateGeometry(mDrawable);
}
