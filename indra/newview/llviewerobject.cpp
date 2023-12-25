/**
 * @file llviewerobject.cpp
 * @brief Base class for viewer objects
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

#include <sstream>
#include <utility>

#include "boost/lexical_cast.hpp"

#include "llviewerobject.h"

#include "imageids.h"
#include "llaudioengine.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llerrorcontrol.h"				// For LLError::getTagLevel()
#include "llfasttimer.h"
#include "llglslshader.h"				// For gUsePBRShaders
#include "llinventory.h"
#include "llmaterialid.h"
#include "llmaterialtable.h"
#include "llnamevalue.h"
#include "llprimitive.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lltree_common.h"
#include "llvolume.h"
#include "llvolumemessage.h"
#include "llxfermanager.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llaudiosourcevo.h"
#include "lldrawable.h"
#include "llface.h"
#include "llflexibleobject.h"
#include "hbfloaterdebugtags.h"			// For HBFloaterDebugTags::setTag()
#include "llfloaterproperties.h"
#include "llfloatertools.h"
#include "llfollowcam.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"
#include "llhudtext.h"
#include "llinventorymodel.h"
#include "lllocalbitmaps.h"
#include "llmanip.h"
#include "llmutelist.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"			// For gGenericDispatcher;
#include "llviewerobjectlist.h"
#include "llviewerparceloverlay.h"
#include "llviewerpartsim.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llviewertextureanim.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvoclouds.h"
#include "llvograss.h"
#include "llvopartgroup.h"
#include "llvosky.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowater.h"
#include "llvowlsky.h"
#include "llworld.h"

using namespace LLAvatarAppearanceDefines;

// At 45 Hz collisions seem stable and objects seem to settle down at a
// reasonable rate. JC 3/18/2003
constexpr F32 PHYSICS_TIMESTEP  = 1.f / 45.f;
// Maximum number of unknown tokens in inventory files
constexpr U32 MAX_INV_FILE_READ_FAILS = 25;

//constexpr U8 LL_SOUND_FLAG_NONE =        0x0;
constexpr U8 LL_SOUND_FLAG_LOOP =         1 << 0;
constexpr U8 LL_SOUND_FLAG_SYNC_MASTER =  1 << 1;
constexpr U8 LL_SOUND_FLAG_SYNC_SLAVE =   1 << 2;
//constexpr U8 LL_SOUND_FLAG_SYNC_PENDING = 1 << 3;
constexpr U8 LL_SOUND_FLAG_QUEUE =        1 << 4;
constexpr U8 LL_SOUND_FLAG_STOP =         1 << 5;

// The maximum size of an object extra parameters binary (packed) block
#define MAX_OBJECT_PARAMS_SIZE 1024

bool LLViewerObject::sVelocityInterpolate = true;
bool LLViewerObject::sPingInterpolate = true;

// For motion interpolation: after X seconds with no updates, don't predict
// object motion. NOTE: sMaxUpdateInterpolationTime must be greater than
// sPhaseOutUpdateInterpolationTime.
F64	 LLViewerObject::sMaxUpdateInterpolationTime = 3.0;
// For motion interpolation: after Y seconds with no updates, taper off motion
// prediction
F64 LLViewerObject::sPhaseOutUpdateInterpolationTime = 1.0;
// For motion interpolation: do not interpolate over this time on region
// crossing
F64 LLViewerObject::sMaxRegionCrossingInterpolationTime = 1.0;

S32 LLViewerObject::sNumObjects = 0;
bool LLViewerObject::sUseNewTargetOmegaCode = false;

std::map<std::string, U32> LLViewerObject::sObjectDataMap;

LLUUID LLViewerObject::sDebugObjectId;

#if LL_ANIMESH_VPARAMS
///////////////////////////////////////////////////////////////////////////////
// Helper class, purely static (pointless singleton in LL's implementation)
class LLObjectExtendedAttributesMap
{
public:
	LL_INLINE static void setAttributes(const LLUUID& obj_id, const LLSD& msg)
	{
		sObjectsMap[obj_id] = msg;
	}

	static LLSD getField(const LLUUID& obj_id, const std::string& field_name)
	{
		LLSD result;
		map_t::const_iterator it = sObjectsMap.find(obj_id);
		if (it != sObjectsMap.end())
		{
			if (it->second.has(field_name))
			{
				result = it->second.get(field_name);
			}
		}
		return result;
	}

	static const LLSD& getData(const LLUUID& obj_id)
	{
		static const LLSD empty;
		map_t::const_iterator it = sObjectsMap.find(obj_id);
		return it != sObjectsMap.end() ? it->second : empty;
	}

	static void forgetObject(const LLUUID& obj_id)
	{
		sObjectsMap.erase(obj_id);
	}

private:
	typedef fast_hmap<LLUUID, LLSD> map_t;
	static map_t sObjectsMap;
};

//static
LLObjectExtendedAttributesMap::map_t LLObjectExtendedAttributesMap::sObjectsMap;
///////////////////////////////////////////////////////////////////////////////
#endif

//static
LLViewerObject* LLViewerObject::createObject(const LLUUID& id, LLPCode pcode,
											 LLViewerRegion* regionp,
											 S32 flags)
{
	LL_FAST_TIMER(FTM_CREATE_OBJECT);

	LLViewerObject* res = NULL;

	switch (pcode)
	{
		case LL_PCODE_VOLUME:
			res = new LLVOVolume(id, regionp);
			break;

		case LL_PCODE_LEGACY_AVATAR:
		{
			if (id == gAgentID)
			{
				if (!gAgentAvatarp)
				{
					gAgentAvatarp = new LLVOAvatarSelf(id, regionp);
					gAgentAvatarp->initInstance();
				}
				else if (isAgentAvatarValid())
				{
					gAgentAvatarp->updateRegion(regionp);
				}
				res = gAgentAvatarp;
			}
			else if (flags & CO_FLAG_UI_AVATAR)
			{
				LLVOAvatarUI* avatar = new LLVOAvatarUI(id, regionp);
				avatar->initInstance();
				res = avatar;
			}
			else if (flags & CO_FLAG_PUPPET_AVATAR)
			{
				LLVOAvatarPuppet* avatar = new LLVOAvatarPuppet(id, regionp);
				avatar->initInstance();
				res = avatar;
			}
			else
			{
				LLVOAvatar* avatar = new LLVOAvatar(id, regionp);
				avatar->initInstance();
				res = avatar;
			}
			break;
		}

		case LL_PCODE_LEGACY_GRASS:
			res = new LLVOGrass(id, regionp);
			break;

		case LL_PCODE_LEGACY_TREE:
			res = new LLVOTree(id, regionp);
			break;

		case LL_VO_CLOUDS:
			res = new LLVOClouds(id, regionp);
			break;

		case LL_VO_SURFACE_PATCH:
			res = new LLVOSurfacePatch(id, regionp);
			break;

		case LL_VO_SKY:
			res = new LLVOSky(id, regionp);
			break;

		case LL_VO_VOID_WATER:
			res = new LLVOVoidWater(id, regionp);
			break;

		case LL_VO_WATER:
			res = new LLVOWater(id, regionp);
			break;

		case LL_VO_PART_GROUP:
			res = new LLVOPartGroup(id, regionp);
			break;

		case LL_VO_HUD_PART_GROUP:
			res = new LLVOHUDPartGroup(id, regionp);
			break;

		case LL_VO_WL_SKY:
			res = new LLVOWLSky(id, regionp);
			break;

		default:
			llwarns_once << "Unknown or deperecated object pcode: "
						 << (S32)pcode << llendl;
	}

	return res;
}

LLViewerObject::LLViewerObject(const LLUUID& id, LLPCode pcode,
							   LLViewerRegion* regionp, bool is_global)
:	mID(id),
	mLocalID(0),
	mTotalCRC(0),
	mListIndex(-1),
	mCanSelect(true),
	mFlags(0),
	mFlagsLoaded(false),
	mPhysicsShapeType(0),
	mPhysicsGravity(0),
	mPhysicsFriction(0),
	mPhysicsDensity(0),
	mPhysicsRestitution(0),
	mCreateSelected(false),
	mIsReflectionProbe(false),
	mBestUpdatePrecision(0),
	mLastInterpUpdateSecs(LLFrameTimer::getElapsedSeconds()),
	mRegionCrossExpire(0.0),
	mLastMessageUpdateSecs(0.0),
	mLatestRecvPacketID(0),
	mData(NULL),
	mAudioSourcep(NULL),
	mAudioGain(1.f),
	mSoundCutOffRadius(0.f),
	mAppAngle(0.f),
	mPixelArea(1024.f),
	mInventory(NULL),
	mInventorySerialNum(0),
	mExpectedInventorySerialNum(0),
	mInvRequestState(INVENTORY_REQUEST_STOPPED),
	mInvRequestXFerId(0),
	mInventoryDirty(false),
	mRegionp(regionp),
	mDead(false),
	mOrphaned(false),
	mUserSelected(false),
	mOnActiveList(false),
	mOnMap(false),
	mStatic(false),
	mNumFaces(0),
	mRotTime(0.f),
	mAttachmentState(0),
	mMedia(NULL),
	mClickAction(0),
	mObjectCost(0.f),
	mLinksetCost(0.f),
	mPhysicsCost(0.f),
	mLinksetPhysicsCost(0.f),
	mCostStale(true),
	mShouldShrinkWrap(false),
	mPhysicsShapeUnknown(true),
	mAttachmentItemID(LLUUID::null),
	mLastUpdateType(OUT_UNKNOWN),
	mLastUpdateCached(false)
{
	if (!is_global)
	{
		llassert(mRegionp);
	}

	setPCode(pcode);

	// Initialize the extra parameters data arrays to NULL pointers and false
	// 'in use' booleans.
	memset((void*)&mExtraParameters[0], 0,
			LL_EPARAMS_COUNT * sizeof(LLNetworkData*));
	memset((void*)&mExtraParameterInUse[0], (int)false,
			LL_EPARAMS_COUNT * sizeof(bool));

	mDebugUpdateMsg = id == sDebugObjectId;
	if (mDebugUpdateMsg)
	{
		llinfos << "Debugged object created with Id: " << id << llendl;
	}

	if (!is_global && mRegionp)
	{
		mPositionAgent = mRegionp->getOriginAgent();
	}

	if (sUseNewTargetOmegaCode)
	{
		resetRot();
	}

	++sNumObjects;
}

LLViewerObject::~LLViewerObject()
{
	if (!mDead)		// Paranoia
	{
		llwarns << "Object " << std::hex << intptr_t(this) << std::dec
				<< " destroyed while not yet marked dead." << llendl;
		llassert(false);
		markDead();
	}

	deleteTEImages();

	// Unhook from reflection probe manager
	if (mReflectionProbe.notNull())
	{
		mReflectionProbe->mViewerObject = NULL;
		mReflectionProbe = NULL;
	}

	if (mInventory)
	{
		mInventory->clear();  // Will de-reference and delete entries
		delete mInventory;
		mInventory = NULL;
	}

	if (mPartSourcep)
	{
		mPartSourcep->setDead();
		mPartSourcep = NULL;
	}

	// Delete memory associated with extra parameters.
	for (S32 i = 0; i < LL_EPARAMS_COUNT; ++i)
	{
		LLNetworkData* param = mExtraParameters[i];
		if (param)
		{
			delete param;
		}
	}

	std::for_each(mNameValuePairs.begin(), mNameValuePairs.end(),
				  DeletePairedPointer());
	mNameValuePairs.clear();

	mJointRiggingInfoTab.clear();

	delete[] mData;
	mData = NULL;

	delete mMedia;
	mMedia = NULL;

	--sNumObjects;
	llassert(mChildList.size() == 0);

	clearInventoryListeners();
}

void LLViewerObject::deleteTEImages()
{
	mTEImages.clear();
	mTENormalMaps.clear();
	mTESpecularMaps.clear();
}

void LLViewerObject::markDead()
{
	if (mDead)
	{
		return;
	}

	if (mUserSelected)
	{
		gSelectMgr.deselectObjectAndFamily(this);
	}

#if LL_ANIMESH_VPARAMS
	LLObjectExtendedAttributesMap::forgetObject(mID);
#endif

	// Do this before the following removeChild()...
	LLVOAvatar* av = getAvatar();

	// Root object of this hierarchy unlinks itself.
	if (getParent())
	{
		((LLViewerObject*)getParent())->removeChild(this);
	}

	LLUUID mesh_id;
	if (av && LLVOAvatar::getRiggedMeshID(this, mesh_id))
	{
		// This case is needed for indirectly attached mesh objects.
		av->updateAttachmentOverrides();
	}

	LLVOInventoryListener::removeObjectFromListeners(this);

	// Mark itself as dead
	mDead = true;
	if (mRegionp)
	{
		mRegionp->removeFromCreatedList(getLocalID());
	}
	gObjectList.cleanupReferences(this);

	while (mChildList.size() > 0)
	{
		LLViewerObject* childp = mChildList.back();
		if (childp->isAvatar())
		{
			// Make sure avatar is no longer parented, so we can properly set
			// its position
			childp->setDrawableParent(NULL);

			LLVOAvatar* avatarp = (LLVOAvatar*)childp;
			if (avatarp->isSelf())
			{
				LL_DEBUGS("AgentSit") << "Unsitting agent from dead object"
									  << LL_ENDL;
			}
			avatarp->getOffObject();

			childp->setParent(NULL);
		}
		else
		{
			childp->setParent(NULL);
			childp->markDead();
		}
		mChildList.pop_back();
	}

	if (mDrawable.notNull())
	{
		// Drawables are reference counted, mark as dead, then nuke the
		// pointer.
		mDrawable->markDead();
		mDrawable = NULL;
	}

	// Unhook from reflection probe manager
	if (mReflectionProbe.notNull())
	{
		mReflectionProbe->mViewerObject = NULL;
		mReflectionProbe = NULL;
	}

	if (mText)
	{
		mText->markDead();
		mText = NULL;
	}

	if (mIcon)
	{
		mIcon->markDead();
		mIcon = NULL;
	}

	if (mPartSourcep)
	{
		mPartSourcep->setDead();
		mPartSourcep = NULL;
	}

	if (mAudioSourcep)
	{
		// Do some cleanup
		if (gAudiop)
		{
			gAudiop->cleanupAudioSource(mAudioSourcep);
		}
		mAudioSourcep = NULL;
	}

	if (flagAnimSource())
	{
		if (isAgentAvatarValid())
		{
			// Stop motions associated with this object
			gAgentAvatarp->stopMotionFromSource(mID);
		}
	}

	if (flagCameraSource())
	{
		LLFollowCamMgr::removeFollowCamParams(mID);
	}

	// Do this last, since this will destroy ourselves if we are the puppet
	// avatar object...
	if (av && av->isPuppetAvatar())
	{
		unlinkPuppetAvatar();
	}
}

void LLViewerObject::dump() const
{
	llinfos << "Type: " << pCodeToString(mPrimitiveCode) << llendl;
	llinfos << "Drawable: " << (LLDrawable *)mDrawable << llendl;
	llinfos << "Update Age: "
			<< LLFrameTimer::getElapsedSeconds() - mLastMessageUpdateSecs
			<< llendl;

	llinfos << "Parent: " << getParent() << llendl;
	llinfos << "ID: " << mID << llendl;
	llinfos << "LocalID: " << mLocalID << llendl;
	llinfos << "PositionRegion: " << getPositionRegion() << llendl;
	llinfos << "PositionAgent: " << getPositionAgent() << llendl;
	llinfos << "PositionGlobal: " << getPositionGlobal() << llendl;
	llinfos << "Velocity: " << getVelocity() << llendl;
	llinfos << "Angular velocity: " << getAngularVelocity() << llendl;
	if (mDrawable.notNull() && mDrawable->getNumFaces() &&
		mDrawable->getFace(0))
	{
		LLFacePool* poolp = mDrawable->getFace(0)->getPool();
		if (poolp)
		{
			llinfos << "Pool: " << poolp << llendl;
			llinfos << "Pool reference count: " << poolp->mReferences.size()
					<< llendl;
		}
		else
		{
			llinfos << "No pool for this object." << llendl;
		}
	}
#if 0
	llinfos << "BoxTree Min: " << mDrawable->getBox()->getMin() << llendl;
	llinfos << "BoxTree Max: " << mDrawable->getBox()->getMin() << llendl;
	llinfos << "Velocity: " << getVelocity() << llendl;
	llinfos << "AnyOwner: " << permAnyOwner() << " YouOwner: "
			<< permYouOwner() << " Edit: " << mPermEdit << llendl;
	llinfos << "UsePhysics: " << flagUsePhysics() << " CanSelect "
			<< mCanSelect << " UserSelected " << mUserSelected << llendl;
	llinfos << "AppAngle: " << mAppAngle << llendl;
	llinfos << "PixelArea: " << mPixelArea << llendl;

	char buffer[1000];
	for (char* key = mNameValuePairs.getFirstKey(); key;
		 key = mNameValuePairs.getNextKey())
	{
		mNameValuePairs[key]->printNameValue(buffer);
		llinfos << buffer << llendl;
	}
	for (child_list_t::iterator iter = mChildList.begin();
		 iter != mChildList.end(); ++iter)
	{
		LLViewerObject* child = *iter;
		llinfos << "  child " << child->mID << llendl;
	}
#endif
}

void LLViewerObject::printNameValuePairs() const
{
	for (name_value_map_t::const_iterator iter = mNameValuePairs.begin(),
										  end = mNameValuePairs.end();
		 iter != end; ++iter)
	{
		LLNameValue* nv = iter->second;
		llinfos << nv->printNameValue() << llendl;
	}
}

void LLViewerObject::initVOClasses()
{
	sPingInterpolate = gSavedSettings.getBool("PingInterpolate");
	sVelocityInterpolate = gSavedSettings.getBool("VelocityInterpolate");
	setUpdateInterpolationTimes(gSavedSettings.getF32("InterpolationTime"),
								gSavedSettings.getF32("InterpolationPhaseOut"),
								gSavedSettings.getF32("RegionCrossingInterpolationTime"));
	sDebugObjectId.set(gSavedSettings.getString("DebugObjectId"), false);
	if (sDebugObjectId.notNull())
	{
		llinfos << "Debugging enabled on object Id: " << sDebugObjectId
				<< llendl;
	}

	// New, experimental code paths toggles (get rid of them once confirmed):
	sUseNewTargetOmegaCode = gSavedSettings.getBool("UseNewTargetOmegaCode");

	// Initialized shared class stuff first.
	LLVOAvatar::initClass();
	LLVOTree::initClass();
	llinfos << "LLViewerObject size: " << sizeof(LLViewerObject) << llendl;
	LLVOGrass::initClass();
	LLVOWater::initClass();
	LLVOVolume::initClass();
	LLVOWLSky::initClass();

	LLVolumeImplFlexible::sUpdateFactor =
		gSavedSettings.getF32("RenderFlexTimeFactor");

	LLVOCacheEntry::updateSettings();

	initObjectDataMap();
}

void LLViewerObject::cleanupVOClasses()
{
	LLVOWLSky::cleanupClass();
	LLVOGrass::cleanupClass();
	LLVOWater::cleanupClass();
	LLVOTree::cleanupClass();
	LLVOAvatar::cleanupClass();
	LLVOVolume::cleanupClass();

	sObjectDataMap.clear();
}

void LLViewerObject::toggleDebugUpdateMsg()
{
	mDebugUpdateMsg = !mDebugUpdateMsg;
	llinfos << "Debugging " << (mDebugUpdateMsg ? "enabled" : "disabled")
			<< " on object Id: " << mID << llendl;
}

void LLViewerObject::setlocalID(U32 local_id)
{
	if (mLocalID != local_id)
	{
		mLocalID = local_id;
		if (sDebugObjectId == mID)
		{
			llinfos << "Received local Id " << local_id << " for object "
					<< mID << llendl;
		}
	}
}

//static
void LLViewerObject::setDebugObjectId(const LLUUID& id)
{
	bool changed = id != sDebugObjectId;
	if (changed && sDebugObjectId.notNull())
	{
		LLViewerObject* objectp = gObjectList.findObject(sDebugObjectId);
		if (objectp)
		{
			objectp->mDebugUpdateMsg = false;
		}
	}
	sDebugObjectId = id;
	if (id.isNull())
	{
		return;
	}
	if (changed)
	{
		llinfos << "Debugging enabled on object Id: " << id << llendl;
	}
}

// Object data map for compressed && !OUT_TERSE_IMPROVED
//static
void LLViewerObject::initObjectDataMap()
{
	U32 count = 0;

	sObjectDataMap["ID"] = count;
	count += sizeof(LLUUID);

	sObjectDataMap["LocalID"] = count;
	count += sizeof(U32);

	sObjectDataMap["PCode"] = count;
	count += sizeof(U8);

	sObjectDataMap["State"] = count;
	count += sizeof(U8);

	sObjectDataMap["CRC"] = count;
	count += sizeof(U32);

	sObjectDataMap["Material"] = count;
	count += sizeof(U8);

	sObjectDataMap["ClickAction"] = count;
	count += sizeof(U8);

	sObjectDataMap["Scale"] = count;
	count += sizeof(LLVector3);

	sObjectDataMap["Pos"] = count;
	count += sizeof(LLVector3);

	sObjectDataMap["Rot"] = count;
	count += sizeof(LLVector3);

	sObjectDataMap["SpecialCode"] = count;
	count += sizeof(U32);

	sObjectDataMap["Owner"] = count;
	count += sizeof(LLUUID);

	// LLVector3, when SpecialCode & 0x80 is set
	sObjectDataMap["Omega"] = count;
	count += sizeof(LLVector3);

	// ParentID is after Omega if there is Omega, otherwise is after Owner.
	// U32, when SpecialCode & 0x20 is set
	sObjectDataMap["ParentID"] = count;
	count += sizeof(U32);

	// The rest items are not included here
}

//static
void LLViewerObject::unpackVector3(LLDataPackerBinaryBuffer* dp,
								   LLVector3& value, std::string name)
{
	dp->shift(sObjectDataMap[name]);
	dp->unpackVector3(value, name.c_str());
	dp->reset();
}

//static
void LLViewerObject::unpackUUID(LLDataPackerBinaryBuffer* dp, LLUUID& value,
								std::string name)
{
	dp->shift(sObjectDataMap[name]);
	dp->unpackUUID(value, name.c_str());
	dp->reset();
}

//static
void LLViewerObject::unpackU32(LLDataPackerBinaryBuffer* dp, U32& value,
							   std::string name)
{
	dp->shift(sObjectDataMap[name]);
	dp->unpackU32(value, name.c_str());
	dp->reset();
}

//static
void LLViewerObject::unpackU8(LLDataPackerBinaryBuffer* dp, U8& value,
							  std::string name)
{
	dp->shift(sObjectDataMap[name]);
	dp->unpackU8(value, name.c_str());
	dp->reset();
}

//static
U32 LLViewerObject::unpackParentID(LLDataPackerBinaryBuffer* dp,
								   U32& parent_id)
{
	dp->shift(sObjectDataMap["SpecialCode"]);
	U32 value;
	dp->unpackU32(value, "SpecialCode");

	parent_id = 0;
	if (value & 0x20)
	{
		S32 offset = sObjectDataMap["ParentID"];
		if (!(value & 0x80))
		{
			offset -= sizeof(LLVector3);
		}

		dp->shift(offset);
		dp->unpackU32(parent_id, "ParentID");
	}
	dp->reset();

	return parent_id;
}

// Replaces all name value pairs with data from \n delimited list. Does not
// update the server.
void LLViewerObject::setNameValueList(const std::string& name_value_list)
{
	// Clear out the old
	for_each(mNameValuePairs.begin(), mNameValuePairs.end(),
			 DeletePairedPointer());
	mNameValuePairs.clear();

	// Bring in the new
	std::string::size_type length = name_value_list.length();
	std::string::size_type start = 0;
	std::string::size_type end;
	while (start < length)
	{
		end = name_value_list.find_first_of("\n", start);
		if (end == std::string::npos)
		{
			end = length;
		}
		if (end > start)
		{
			std::string tok = name_value_list.substr(start, end - start);
			addNVPair(tok);
		}
		start = end + 1;
	}
}

// This method returns true if the object is over land owned by the agent.
bool LLViewerObject::isReturnable()
{
	if (isAttachment())
	{
		return false;
	}
	std::vector<LLBBox> boxes;
	boxes.push_back(LLBBox(getPositionRegion(), getRotationRegion(),
					getScale() * -0.5f, getScale() * 0.5f).getAxisAligned());
	for (child_list_t::iterator iter = mChildList.begin(),
								end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		if (!child)	return false;	// Paranoia

		boxes.push_back(LLBBox(child->getPositionRegion(),
							   child->getRotationRegion(),
							   child->getScale() * -0.5f,
							   child->getScale() * 0.5f).getAxisAligned());
	}

	bool result = mRegionp && mRegionp->objectIsReturnable(getPositionRegion(),
														   boxes);

	if (!result)
	{
		// Get list of neighboring regions relative to this VO's region
		std::vector<LLViewerRegion*> unique_regions;
		mRegionp->getNeighboringRegions(unique_regions);

		// Build AABB's for root and all children
		returnable_vec_t returnables;
		for (std::vector<LLViewerRegion*>::iterator
				reg_it = unique_regions.begin(),
				reg_end = unique_regions.end();
			 reg_it != reg_end; ++reg_it)
		{
			LLViewerRegion* target_regionp = *reg_it;
			// Add the root vo as there may be no children and we still want
			// to test for any edge overlap
			buildReturnablesForChildrenVO(returnables, this, target_regionp);
			// Add its children
			for (child_list_t::iterator iter = mChildList.begin(),
										end = mChildList.end();
				 iter != end; ++iter)
			{
				LLViewerObject* childp = *iter;
				buildReturnablesForChildrenVO(returnables, childp,
											  target_regionp);
			}
		}

		// TBD: eventually create a region -> box list map
		for (returnable_vec_t::iterator it = returnables.begin(),
										end = returnables.end();
			 it != end; ++it)
		{
			boxes.clear();
			LLViewerRegion* regionp = it->region;
			boxes.push_back(it->box);
			if (regionp && regionp->childrenObjectReturnable(boxes) &&
				regionp->canManageEstate())
			{
				result = true;
				break;
			}
		}
	}

	return result;
}

void LLViewerObject::buildReturnablesForChildrenVO(returnable_vec_t& returnables,
												   LLViewerObject* childp,
												   LLViewerRegion* target_regionp)
{
	if (!childp)
	{
		llerrs << "Child viewerobject is NULL" << llendl;
	}

	constructAndAddReturnable(returnables, childp, target_regionp);

	// We want to handle any children VO's as well
	for (child_list_t::iterator iter = childp->mChildList.begin(),
								end = childp->mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* obj = *iter;
		buildReturnablesForChildrenVO(returnables, obj, target_regionp);
	}
}

void LLViewerObject::constructAndAddReturnable(returnable_vec_t& returnables,
											   LLViewerObject* childp,
											   LLViewerRegion* target_regionp)
{
	LLVector3 target_region_pos;
	target_region_pos.set(childp->getPositionGlobal());

	LLBBox child_bbox = LLBBox(target_region_pos, childp->getRotationRegion(),
							   childp->getScale() * -0.5f,
							   childp->getScale() * 0.5f).getAxisAligned();

	LLVector3 edge_a = target_region_pos + child_bbox.getMinLocal();
	LLVector3 edge_b = target_region_pos + child_bbox.getMaxLocal();

	LLVector3d edge_a_3d, edge_b_3d;
	edge_a_3d.set(edge_a);
	edge_b_3d.set(edge_b);

	// Only add the box when either of the extents are in a neighboring region
	if (target_regionp->pointInRegionGlobal(edge_a_3d) ||
		target_regionp->pointInRegionGlobal(edge_b_3d))
	{
		PotentialReturnableObject returnable_obj;
		returnable_obj.box = child_bbox;
		returnable_obj.region = target_regionp;
		returnables.push_back(returnable_obj);
	}
}

bool LLViewerObject::setParent(LLViewerObject* parent)
{
	if (mParent != parent)
	{
		LLViewerObject* old_parent = (LLViewerObject*)mParent;
		bool ret = LLPrimitive::setParent(parent);
		if (ret && old_parent && parent)
		{
			old_parent->removeChild(this);
		}
		return ret;
	}

	return false;
}

void LLViewerObject::addChild(LLViewerObject* childp)
{
	for (child_list_t::iterator i = mChildList.begin(), end = mChildList.end();
		 i != end; ++i)
	{
		if (*i == childp)
		{
			// Already has child
			return;
		}
	}

	if (!isAvatar())
	{
		// Propagate selection properties
		childp->mCanSelect = mCanSelect;
	}

	if (childp->setParent(this))
	{
		mChildList.push_back(childp);
		childp->afterReparent();
	}
}

void LLViewerObject::removeChild(LLViewerObject* childp)
{
	if (!childp) return;

	for (child_list_t::iterator i = mChildList.begin(), end = mChildList.end();
		 i != end; ++i)
	{
		if (*i == childp)
		{
			if (!childp->isAvatar() && mDrawable.notNull() &&
				mDrawable->isActive() && childp->mDrawable.notNull() &&
				!isAvatar())
			{
				gPipeline.markRebuild(childp->mDrawable,
									  LLDrawable::REBUILD_VOLUME);
			}

			mChildList.erase(i);

			if (childp->getParent() == this)
			{
				childp->setParent(NULL);
			}
			break;
		}
	}

	if (childp->mUserSelected)
	{
		gSelectMgr.deselectObjectAndFamily(childp);
		gSelectMgr.selectObjectAndFamily(childp, true);
	}
}

void LLViewerObject::addThisAndAllChildren(std::vector<LLViewerObject*>& objects)
{
	objects.push_back(this);
	for (child_list_t::iterator iter = mChildList.begin(),
								end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		if (child && !child->isAvatar())
		{
			child->addThisAndAllChildren(objects);
		}
	}
}

void LLViewerObject::addThisAndNonJointChildren(std::vector<LLViewerObject*>& objects)
{
	objects.push_back(this);
	// Do not add any attachments when temporarily selecting avatar
	if (isAvatar())
	{
		return;
	}
	for (child_list_t::iterator iter = mChildList.begin(),
								end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		if (child && !child->isAvatar())
		{
			child->addThisAndNonJointChildren(objects);
		}
	}
}

bool LLViewerObject::isChild(LLViewerObject* childp) const
{
	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* testchild = *iter;
		if (testchild == childp)
		{
			return true;
		}
	}
	return false;
}

bool LLViewerObject::isSeat() const
{
	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		if (child && child->isAvatar())
		{
			return true;
		}
	}
	return false;

}

bool LLViewerObject::isAgentSeat() const
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->mIsSitting)
	{
		// Agent is not even sitting, so do not bother to check further
		return false;
	}

	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		if (child && child == (LLViewerObject*)gAgentAvatarp)
		{
			return true;
		}
	}

	return false;
}

bool LLViewerObject::setDrawableParent(LLDrawable* parentp)
{
	if (mDrawable.isNull())
	{
		return false;
	}

	bool ret = mDrawable->mXform.setParent(parentp ? &parentp->mXform : NULL);
	if (!ret)
	{
		return false;
	}

	LLDrawable* old_parent = mDrawable->mParent;
	mDrawable->mParent = parentp;
	if (parentp && mDrawable->isActive())
	{
		parentp->makeActive();
		parentp->setState(LLDrawable::ACTIVE_CHILD);
	}

	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	if ((old_parent != parentp && old_parent) ||
		(parentp && parentp->isActive()))
	{
		// *TODO: we should not be relying on setDrawable parent to call
		// markMoved()
		gPipeline.markMoved(mDrawable, false);
	}
	else if (!mDrawable->isAvatar())
	{
		mDrawable->updateXform(true);
#if 0
		if (!mDrawable->getSpatialGroup())
		{
			mDrawable->movePartition();
		}
#endif
	}

	return ret;
}

// Show or hide particles, icon and HUD
void LLViewerObject::hideExtraDisplayItems(bool hidden)
{
	if (mPartSourcep.notNull())
	{
		LLViewerPartSourceScript* ps = mPartSourcep.get();
		ps->setSuspended(hidden);
	}

	if (mText.notNull())
	{
		LLHUDText* hudtext = mText.get();
		hudtext->setHidden(hidden);
	}

	if (mIcon.notNull())
	{
		LLHUDIcon* hudicon = mIcon.get();
		hudicon->setHidden(hidden);
	}
}

U32 LLViewerObject::checkMediaURL(const std::string& media_url)
{
	U32 retval = 0;
	if (!mMedia && !media_url.empty())
	{
		retval |= MEDIA_URL_ADDED;
		mMedia = new LLViewerObjectMedia;
		mMedia->mMediaURL = media_url;
		mMedia->mMediaType = LLViewerObject::MEDIA_SET;
		mMedia->mPassedWhitelist = false;
	}
	else if (mMedia)
	{
		if (media_url.empty())
		{
			retval |= MEDIA_URL_REMOVED;
			delete mMedia;
			mMedia = NULL;
		}
		else if (mMedia->mMediaURL != media_url)
		{
			// We just added or changed a media.
			retval |= MEDIA_URL_UPDATED;
			mMedia->mMediaURL = media_url;
			mMedia->mPassedWhitelist = false;
		}
	}
	return retval;
}

// Extracts spatial information from object update message. Return parent_id.
//static
U32 LLViewerObject::extractSpatialExtents(LLDataPackerBinaryBuffer* dp,
										  LLVector3& pos, LLVector3& scale,
										  LLQuaternion& rot)
{
	U32	parent_id = 0;
	LLViewerObject::unpackParentID(dp, parent_id);

	LLViewerObject::unpackVector3(dp, scale, "Scale");
	LLViewerObject::unpackVector3(dp, pos, "Pos");

	LLVector3 vec;
	LLViewerObject::unpackVector3(dp, vec, "Rot");
	rot.unpackFromVector3(vec);

	return parent_id;
}

U32 LLViewerObject::processUpdateMessage(LLMessageSystem* mesgsys,
										 void** user_data, U32 block_num,
										 EObjectUpdateType update_type,
										 LLDataPacker* dp)
{
	U32 retval = 0x0;

	// If region is removed from the list it is also deleted.
	if (!gWorld.isRegionListed(mRegionp))
	{
		llwarns << "Updating object in an invalid region" << llendl;
		return retval;
	}

	// Coordinates of objects on simulators are region-local.
	U64 region_handle;
	if (mesgsys)
	{
		mesgsys->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle,
							region_handle);
		LLViewerRegion* regionp = gWorld.getRegionFromHandle(region_handle);
		if (regionp != mRegionp && regionp && mRegionp)	// region cross
		{
			// This is the redundant position and region update, but it is
			// necessary in case the viewer misses the following position and
			// region update messages from sim. This redundant update should
			// not cause any problems.
			LLVector3 delta_pos = mRegionp->getOriginAgent() -
								  regionp->getOriginAgent();
			// Update to the new region position immediately:
			setPositionParent(getPosition() + delta_pos);
			// Change the region:
			setRegion(regionp);
		}
		else if (regionp != mRegionp)
		{
			if (mRegionp)
			{
				mRegionp->removeFromCreatedList(getLocalID());
			}
			if (regionp)
			{
				regionp->addToCreatedList(getLocalID());
			}
			mRegionp = regionp;
		}
	}

	if (!mRegionp)
	{
		U32 x, y;
		from_region_handle(region_handle, &x, &y);

		llerrs << "Object has invalid region " << x << ":" << y << llendl;
		return retval;
	}

	if (mesgsys)
	{
		U16 time_dilation16;
		mesgsys->getU16Fast(_PREHASH_RegionData, _PREHASH_TimeDilation,
							time_dilation16);
		F32 time_dilation = ((F32)time_dilation16) / 65535.f;
		mRegionp->setTimeDilation(time_dilation);
	}

	// This will be used to determine if we have really changed position. Use
	// getPosition, not getPositionRegion, since this is what we're comparing
	// directly against.
	LLVector3 test_pos_parent = getPosition();

	// This needs to match the largest size below.
	constexpr S32 MAX_DATA_SIZE = 60 + 16;
	static U8 data[MAX_DATA_SIZE];

#if LL_BIG_ENDIAN
	U16 valswizzle[4];
#endif
	U16* val;

	constexpr F32 MAX_HEIGHT = MAX_OBJECT_Z;
	// Variable region size support:
	const F32 size = mRegionp->getWidth();
	const F32 min_height = -size;

	S32 length,	count;
	S32 this_update_precision = 32;	// in bits

	// Temporaries, because we need to compare with previous to set dirty flags
	LLVector3 new_pos_parent, new_vel, new_acc, new_angv;
	LLVector3 new_scale = getScale();
	LLVector3 old_angv = getAngularVelocity();
	LLQuaternion new_rot;

	U8 material = 0;
	U8 click_action = 0;
	U32 crc = 0;

	U32	parent_id = 0;
	LLViewerObject* cur_parentp = (LLViewerObject*)getParent();
	if (cur_parentp)
	{
		parent_id = cur_parentp->mLocalID;
	}

	if (!dp)
	{
		if (mDebugUpdateMsg)
		{
			llinfos << "Update message received for object " << mID << ":"
					<< llendl;
		}
		switch (update_type)
		{
			case OUT_FULL:
			{
				LL_DEBUGS("UpdateType") << "Full: " << mID << LL_ENDL;

				// Clear cost and linkset cost
				mCostStale = true;
				if (gFloaterToolsp && mUserSelected)
				{
					gFloaterToolsp->dirty();
				}

				mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_CRC, crc,
									block_num);
				mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ParentID,
									parent_id, block_num);
				LLUUID audio_uuid;
				mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_Sound,
									 audio_uuid, block_num);
				// *HACK: Owner Id only valid if non-null sound Id or particle
				// system and if audio_uuid or particle system is not null:
				LLUUID owner_id;
				mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID,
									 owner_id, block_num);
				F32	gain;
				mesgsys->getF32Fast(_PREHASH_ObjectData, _PREHASH_Gain, gain,
									block_num);
				F32 cutoff;
				mesgsys->getF32Fast(_PREHASH_ObjectData, _PREHASH_Radius,
									cutoff, block_num);
				U8 sound_flags;
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_Flags,
								   sound_flags, block_num);
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_Material,
								   material, block_num);
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_ClickAction,
								   click_action, block_num);
				mesgsys->getVector3Fast(_PREHASH_ObjectData, _PREHASH_Scale,
										new_scale, block_num);
				length = mesgsys->getSizeFast(_PREHASH_ObjectData, block_num,
											  _PREHASH_ObjectData);
				mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
										   _PREHASH_ObjectData, data, length,
										   block_num, MAX_DATA_SIZE);

				mTotalCRC = crc;
				// Might need to update mSourceMuted here to properly pick up
				// the new cutoff radius.
				mSoundCutOffRadius = cutoff;

				// Owner Id used for sound muting or particle system muting
				mOwnerID = owner_id;
				setAttachedSound(audio_uuid, owner_id, gain, sound_flags);

				U8 old_material = getMaterial();
				if (old_material != material)
				{
					setMaterial(material);
					if (mDrawable.notNull())
					{
						gPipeline.markMoved(mDrawable, false); // Undamped
					}
				}
				setClickAction(click_action);

				count = 0;
				LLVector4 collision_plane;

				switch (length)
				{
				case (60 + 16):
					// Pull out collision normal for avatar
					htonmemcpy(collision_plane.mV, &data[count], MVT_LLVector4,
							   sizeof(LLVector4));
					if (isAvatar())
					{
						((LLVOAvatar*)this)->setFootPlane(collision_plane);
					}
					count += sizeof(LLVector4);
					// Fall through

				case 60:
				{
					this_update_precision = 32;
					// This is a terse update
					htonmemcpy(new_pos_parent.mV, &data[count], MVT_LLVector3,
							   sizeof(LLVector3));
					count += sizeof(LLVector3);
					htonmemcpy((void*)getVelocity().mV, &data[count],
							   MVT_LLVector3, sizeof(LLVector3));
					count += sizeof(LLVector3);
					htonmemcpy((void*)getAcceleration().mV, &data[count],
							   MVT_LLVector3, sizeof(LLVector3));
					count += sizeof(LLVector3);
					// Theta
					LLVector3 vec;
					htonmemcpy(vec.mV, &data[count], MVT_LLVector3,
							   sizeof(LLVector3));
					new_rot.unpackFromVector3(vec);
					count += sizeof(LLVector3);
					// Target omega
					htonmemcpy((void*)new_angv.mV, &data[count], MVT_LLVector3,
							   sizeof(LLVector3));
					if (new_angv.isExactlyZero())
					{
						resetRot();
					}
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (1): " << new_angv << llendl;
					}
#if LL_DARWIN		// Why is that ???   HB
					if (length == 76)
					{
						setAngularVelocity(LLVector3::zero);
					}
#endif
					break;
				}

				case(32 + 16):
					// Pull out collision normal for avatar
					htonmemcpy(collision_plane.mV, &data[count], MVT_LLVector4,
							   sizeof(LLVector4));
					if (isAvatar())
					{
						((LLVOAvatar*)this)->setFootPlane(collision_plane);
					}
					count += sizeof(LLVector4);
					// Fall through

				case 32:
					this_update_precision = 16;
					test_pos_parent.quantize16(-0.5f * size, 1.5f * size,
											   min_height, MAX_HEIGHT);

					// This is a terse 16 update, so treat data as an array of U16's.
#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					new_pos_parent.mV[VX] = U16_to_F32(val[VX], -0.5f * size,
													   1.5f * size);
					new_pos_parent.mV[VY] = U16_to_F32(val[VY], -0.5f * size,
													   1.5f * size);
					new_pos_parent.mV[VZ] = U16_to_F32(val[VZ], min_height,
													   MAX_HEIGHT);

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					setVelocity(LLVector3(U16_to_F32(val[VX], -size, size),
										  U16_to_F32(val[VY], -size, size),
										  U16_to_F32(val[VZ], -size, size)));

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					setAcceleration(LLVector3(U16_to_F32(val[VX], -size, size),
											  U16_to_F32(val[VY], -size, size),
											  U16_to_F32(val[VZ], -size,
														 size)));

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Quat, 4);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 4;
					new_rot.mQ[VX] = U16_to_F32(val[VX], -1.f, 1.f);
					new_rot.mQ[VY] = U16_to_F32(val[VY], -1.f, 1.f);
					new_rot.mQ[VZ] = U16_to_F32(val[VZ], -1.f, 1.f);
					new_rot.mQ[VW] = U16_to_F32(val[VW], -1.f, 1.f);

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					new_angv.set(U16_to_F32(val[VX], -size, size),
								 U16_to_F32(val[VY], -size, size),
								 U16_to_F32(val[VZ], -size, size));
					if (new_angv.isExactlyZero())
					{
						resetRot();
					}
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (2): " << new_angv << llendl;
					}
					break;

				case 16:
					this_update_precision = 8;
					test_pos_parent.quantize8(-0.5f * size, 1.5f * size,
											  min_height, MAX_HEIGHT);
					// This is a terse 8 update
					new_pos_parent.mV[VX] = U8_to_F32(data[0], -0.5f * size,
													  1.5f * size);
					new_pos_parent.mV[VY] = U8_to_F32(data[1], -0.5f * size,
													  1.5f * size);
					new_pos_parent.mV[VZ] = U8_to_F32(data[2], min_height,
													  MAX_HEIGHT);

					setVelocity(U8_to_F32(data[3], -size, size),
								U8_to_F32(data[4], -size, size),
								U8_to_F32(data[5], -size, size));

					setAcceleration(U8_to_F32(data[6], -size, size),
									U8_to_F32(data[7], -size, size),
									U8_to_F32(data[8], -size, size));

					new_rot.mQ[VX] = U8_to_F32(data[9], -1.f, 1.f);
					new_rot.mQ[VY] = U8_to_F32(data[10], -1.f, 1.f);
					new_rot.mQ[VZ] = U8_to_F32(data[11], -1.f, 1.f);
					new_rot.mQ[VW] = U8_to_F32(data[12], -1.f, 1.f);

					new_angv.set(U8_to_F32(data[13], -size, size),
								 U8_to_F32(data[14], -size, size),
								 U8_to_F32(data[15], -size, size));
					if (new_angv.isExactlyZero())
					{
						resetRot();
					}
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (3): " << new_angv << llendl;
					}
					break;
				}

				////////////////////////////////////////////////////
				// Here we handle data specific to the full message.

				U32 flags;
				mesgsys->getU32Fast(_PREHASH_ObjectData,
									_PREHASH_UpdateFlags, flags, block_num);
				// Clear all but local flags
				mFlags &= FLAGS_LOCAL;
				mFlags |= flags;
				mFlagsLoaded = true;

				U8 state;
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_State, state,
								   block_num);
				mAttachmentState = state;

				// ...new objects that should come in selected need to be added
				// to the selected list
				mCreateSelected = ((flags & FLAGS_CREATE_SELECTED) != 0);

				// Set all name value pairs
				S32 nv_size = mesgsys->getSizeFast(_PREHASH_ObjectData,
												   block_num,
												   _PREHASH_NameValue);
				if (nv_size > 0)
				{
					std::string name_value_list;
					mesgsys->getStringFast(_PREHASH_ObjectData,
										   _PREHASH_NameValue, name_value_list,
										   block_num);
					setNameValueList(name_value_list);
				}

				// Clear out any existing generic data
				if (mData)
				{
					delete[] mData;
				}

				// Check for appended generic data
				S32 data_size = mesgsys->getSizeFast(_PREHASH_ObjectData,
													 block_num, _PREHASH_Data);
				if (data_size <= 0)
				{
					mData = NULL;
				}
				else
				{
					// ...has generic data
					mData = new U8[data_size];
					mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
											   _PREHASH_Data, mData, data_size,
											   block_num);
				}

				// Reset the cached values used for debug info display toggle.
				mHudTextString.clear();
				mHudTextColor = LLColor4U::white;

				S32 text_size = mesgsys->getSizeFast(_PREHASH_ObjectData,
													 block_num, _PREHASH_Text);
				if (text_size > 1)
				{
					// Setup object text
					if (!mText)
					{
						mText = (LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
						mText->setFont(LLFontGL::getFontSansSerif());
						mText->setVertAlignment(LLHUDText::ALIGN_VERT_TOP);
						mText->setMaxLines(-1);
						mText->setSourceObject(this);
						mText->setOnHUDAttachment(isHUDAttachment());
					}

					mesgsys->getStringFast(_PREHASH_ObjectData, _PREHASH_Text,
										   mHudTextString, block_num);

					LLColor4U coloru;
					mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
											   _PREHASH_TextColor, coloru.mV,
											   4, block_num);

					// Alpha was flipped so that it zero encoded better
					coloru.mV[3] = 255 - coloru.mV[3];
					mHudTextColor = LLColor4(coloru);

					// Fading is disabled only when the hovetext is being
					// overridden by debug text.
					if (mText->getDoFade())
					{
						mText->setColor(mHudTextColor);
						mText->setStringUTF8(mHudTextString);
					}
//MK
					mText->mLastMessageText = mHudTextString;
//mk
					setChanged(MOVED | SILHOUETTE);
				}
				else if (mText.notNull())
				{
					mText->markDead();
					mText = NULL;
				}

				std::string media_url;
				mesgsys->getStringFast(_PREHASH_ObjectData, _PREHASH_MediaURL,
									   media_url, block_num);
				retval |= checkMediaURL(media_url);

				//
				// Unpack particle system data
				//
				unpackParticleSource(block_num, owner_id);

				// Mark all extra parameters not used
				for (S32 i = 0; i < LL_EPARAMS_COUNT; ++i)
				{
					mExtraParameterInUse[i] = false;
				}

				// Unpack extra parameters
				S32 size = mesgsys->getSizeFast(_PREHASH_ObjectData, block_num,
												_PREHASH_ExtraParams);
				if (size > 0)
				{
					U8* buffer = new U8[size];
					mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
											   _PREHASH_ExtraParams,
											   buffer, size, block_num);
					LLDataPackerBinaryBuffer dp(buffer, size);

					U8 num_parameters;
					dp.unpackU8(num_parameters, "num_params");
					U8 param_block[MAX_OBJECT_PARAMS_SIZE];
					for (U8 param = 0; param < num_parameters; ++param)
					{
						U16 param_type;
						S32 param_size;
						dp.unpackU16(param_type, "param_type");
						dp.unpackBinaryData(param_block, param_size,
											"param_data");
						LLDataPackerBinaryBuffer dp2(param_block, param_size);
						unpackParameterEntry(param_type, &dp2);
					}
					delete[] buffer;
				}

				for (S32 i = 0; i < LL_EPARAMS_COUNT; ++i)
				{
					if (!mExtraParameterInUse[i])
					{
						parameterChanged(LL_EPARAM_TYPE(i),
										 mExtraParameters[i], false, false);
					}
				}

				U8 joint_type = 0;
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_JointType,
								   joint_type, block_num);
				if (joint_type)
				{
					llwarns_once << "Received deprecated joint data for object "
								 << mID << ". This data will be ignored..."
								 << llendl;
				}
				break;
			}

			case OUT_TERSE_IMPROVED:
			{
				LL_DEBUGS("UpdateType") << "TI:" << mID << LL_ENDL;

				length = mesgsys->getSizeFast(_PREHASH_ObjectData, block_num,
											  _PREHASH_ObjectData);
				mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
										   _PREHASH_ObjectData, data, length,
										   block_num, MAX_DATA_SIZE);
				count = 0;
				LLVector4 collision_plane;

				switch (length)
				{
				case (60 + 16):
					// Pull out collision normal for avatar
					htonmemcpy(collision_plane.mV, &data[count], MVT_LLVector4,
							   sizeof(LLVector4));
					if (isAvatar())
					{
						((LLVOAvatar*)this)->setFootPlane(collision_plane);
					}
					count += sizeof(LLVector4);
					// Fall through

				case 60:
				{
					// This is a terse 32 update
					this_update_precision = 32;
					htonmemcpy(new_pos_parent.mV, &data[count], MVT_LLVector3,
							   sizeof(LLVector3));
					count += sizeof(LLVector3);
					htonmemcpy((void*)getVelocity().mV, &data[count],
							   MVT_LLVector3, sizeof(LLVector3));
					count += sizeof(LLVector3);
					htonmemcpy((void*)getAcceleration().mV, &data[count],
							   MVT_LLVector3, sizeof(LLVector3));
					count += sizeof(LLVector3);
					// Theta
					{
						LLVector3 vec;
						htonmemcpy(vec.mV, &data[count], MVT_LLVector3,
								   sizeof(LLVector3));
						new_rot.unpackFromVector3(vec);
					}
					count += sizeof(LLVector3);
					// Target omega
					htonmemcpy((void*)new_angv.mV, &data[count], MVT_LLVector3,
							   sizeof(LLVector3));
					if (new_angv.isExactlyZero())
					{
						resetRot();
					}
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (4): " << new_angv << llendl;
					}
#if LL_DARWIN		// Why is that ???   HB
					if (length == 76)
					{
						setAngularVelocity(LLVector3::zero);
					}
#endif
					break;
				}

				case (32 + 16):
					// Pull out collision normal for avatar
					htonmemcpy(collision_plane.mV, &data[count], MVT_LLVector4,
							   sizeof(LLVector4));
					if (isAvatar())
					{
						((LLVOAvatar*)this)->setFootPlane(collision_plane);
					}
					count += sizeof(LLVector4);
					// fall through

				case 32:
					// This is a terse 16 update
					this_update_precision = 16;
					test_pos_parent.quantize16(-0.5f * size, 1.5f * size,
											   min_height, MAX_HEIGHT);

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					new_pos_parent.mV[VX] = U16_to_F32(val[VX], -0.5f * size,
													   1.5f * size);
					new_pos_parent.mV[VY] = U16_to_F32(val[VY], -0.5f * size,
													   1.5f * size);
					new_pos_parent.mV[VZ] = U16_to_F32(val[VZ], min_height,
													   MAX_HEIGHT);

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					setVelocity(U16_to_F32(val[VX], -size, size),
								U16_to_F32(val[VY], -size, size),
								U16_to_F32(val[VZ], -size, size));

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 3;
					setAcceleration(U16_to_F32(val[VX], -size, size),
									U16_to_F32(val[VY], -size, size),
									U16_to_F32(val[VZ], -size, size));

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Quat, 8);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					count += sizeof(U16) * 4;
					new_rot.mQ[VX] = U16_to_F32(val[VX], -1.f, 1.f);
					new_rot.mQ[VY] = U16_to_F32(val[VY], -1.f, 1.f);
					new_rot.mQ[VZ] = U16_to_F32(val[VZ], -1.f, 1.f);
					new_rot.mQ[VW] = U16_to_F32(val[VW], -1.f, 1.f);

#if LL_BIG_ENDIAN
					htonmemcpy(valswizzle, &data[count], MVT_U16Vec3, 6);
					val = valswizzle;
#else
					val = (U16*)&data[count];
#endif
					new_angv.set(U16_to_F32(val[VX], -size, size),
								 U16_to_F32(val[VY], -size, size),
								 U16_to_F32(val[VZ], -size, size));
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (5): " << new_angv << llendl;
					}
					break;

				case 16:
					// This is a terse 8 update
					this_update_precision = 8;
					test_pos_parent.quantize8(-0.5f * size, 1.5f * size,
											  min_height, MAX_HEIGHT);
					new_pos_parent.mV[VX] = U8_to_F32(data[0], -0.5f * size,
													  1.5f * size);
					new_pos_parent.mV[VY] = U8_to_F32(data[1], -0.5f * size,
													  1.5f * size);
					new_pos_parent.mV[VZ] = U8_to_F32(data[2], min_height,
													  MAX_HEIGHT);

					setVelocity(U8_to_F32(data[3], -size, size),
								U8_to_F32(data[4], -size, size),
								U8_to_F32(data[5], -size, size));

					setAcceleration(U8_to_F32(data[6], -size, size),
									U8_to_F32(data[7], -size, size),
									U8_to_F32(data[8], -size, size));

					new_rot.mQ[VX] = U8_to_F32(data[9], -1.f, 1.f);
					new_rot.mQ[VY] = U8_to_F32(data[10], -1.f, 1.f);
					new_rot.mQ[VZ] = U8_to_F32(data[11], -1.f, 1.f);
					new_rot.mQ[VW] = U8_to_F32(data[12], -1.f, 1.f);

					new_angv.set(U8_to_F32(data[13], -size, size),
								 U8_to_F32(data[14], -size, size),
								 U8_to_F32(data[15], -size, size));
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (6): " << new_angv << llendl;
					}
					break;
				}

				U8 state;
				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_State, state,
								   block_num);
				mAttachmentState = state;
				break;
			}

			default:
				break;
		}
	}
	else
	{
		// Handle the compressed case
		LLUUID sound_uuid, owner_id;
		F32 gain = 0;
		U8 sound_flags = 0;
		F32 cutoff = 0;
		U16 val[4];

		U8 state;
		dp->unpackU8(state, "State");
		mAttachmentState = state;

		switch (update_type)
		{
			case OUT_TERSE_IMPROVED:
			{
				LL_DEBUGS("UpdateType") << "CompTI:" << mID << LL_ENDL;

				U8 value;
				dp->unpackU8(value, "agent");
				if (value)
				{
					LLVector4 collision_plane;
					dp->unpackVector4(collision_plane, "Plane");
					if (isAvatar())
					{
						((LLVOAvatar*)this)->setFootPlane(collision_plane);
					}
				}
				test_pos_parent = getPosition();
				dp->unpackVector3(new_pos_parent, "Pos");
				dp->unpackU16(val[VX], "VelX");
				dp->unpackU16(val[VY], "VelY");
				dp->unpackU16(val[VZ], "VelZ");
				setVelocity(U16_to_F32(val[VX], -128.f, 128.f),
							U16_to_F32(val[VY], -128.f, 128.f),
							U16_to_F32(val[VZ], -128.f, 128.f));
				dp->unpackU16(val[VX], "AccX");
				dp->unpackU16(val[VY], "AccY");
				dp->unpackU16(val[VZ], "AccZ");
				setAcceleration(U16_to_F32(val[VX], -64.f, 64.f),
								U16_to_F32(val[VY], -64.f, 64.f),
								U16_to_F32(val[VZ], -64.f, 64.f));

				dp->unpackU16(val[VX], "ThetaX");
				dp->unpackU16(val[VY], "ThetaY");
				dp->unpackU16(val[VZ], "ThetaZ");
				dp->unpackU16(val[VS], "ThetaS");
				new_rot.mQ[VX] = U16_to_F32(val[VX], -1.f, 1.f);
				new_rot.mQ[VY] = U16_to_F32(val[VY], -1.f, 1.f);
				new_rot.mQ[VZ] = U16_to_F32(val[VZ], -1.f, 1.f);
				new_rot.mQ[VS] = U16_to_F32(val[VS], -1.f, 1.f);
				dp->unpackU16(val[VX], "AccX");
				dp->unpackU16(val[VY], "AccY");
				dp->unpackU16(val[VZ], "AccZ");
				new_angv.set(U16_to_F32(val[VX], -64.f, 64.f),
							 U16_to_F32(val[VY], -64.f, 64.f),
							 U16_to_F32(val[VZ], -64.f, 64.f));
				setAngularVelocity(new_angv);
				if (mDebugUpdateMsg)
				{
					llinfos << "Angular velocity (7): " << new_angv << llendl;
				}
				break;
			}

			case OUT_FULL_COMPRESSED:
			case OUT_FULL_CACHED:
			{
				LL_DEBUGS("UpdateType") << "CompFull:" << mID << LL_ENDL;

				mCostStale = true;

				if (gFloaterToolsp && mUserSelected)
				{
					gFloaterToolsp->dirty();
				}

				dp->unpackU32(crc, "CRC");
				mTotalCRC = crc;
				dp->unpackU8(material, "Material");
				U8 old_material = getMaterial();
				if (old_material != material)
				{
					setMaterial(material);
					if (mDrawable.notNull())
					{
						gPipeline.markMoved(mDrawable, false); // undamped
					}
				}
				dp->unpackU8(click_action, "ClickAction");
				setClickAction(click_action);
				dp->unpackVector3(new_scale, "Scale");
				dp->unpackVector3(new_pos_parent, "Pos");
				LLVector3 vec;
				dp->unpackVector3(vec, "Rot");
				new_rot.unpackFromVector3(vec);
				setAcceleration(LLVector3::zero);

				U32 value;
				dp->unpackU32(value, "SpecialCode");
				dp->setPassFlags(value);
				dp->unpackUUID(owner_id, "Owner");

				mOwnerID = owner_id;

				if (value & 0x80)
				{
					dp->unpackVector3(new_angv, "Omega");
					setAngularVelocity(new_angv);
					if (mDebugUpdateMsg)
					{
						llinfos << "Angular velocity (8): " << new_angv
								<< llendl;
					}
				}

				if (value & 0x20)
				{
					dp->unpackU32(parent_id, "ParentID");
				}
				else
				{
					parent_id = 0;
				}

				S32 sp_size;
				U32 size;
				if (value & 0x2)
				{
					sp_size = 1;
					delete[] mData;
					mData = new U8[1];
					dp->unpackU8(((U8*)mData)[0], "TreeData");
				}
				else if (value & 0x1)
				{
					dp->unpackU32(size, "ScratchPadSize");
					delete[] mData;
					mData = new U8[size];
					dp->unpackBinaryData((U8 *)mData, sp_size, "PartData");
				}
				else
				{
					mData = NULL;
				}

				// Reset the cached values used for debug info display toggle.
				mHudTextString.clear();
				mHudTextColor = LLColor4U::white;

				// Setup object text
				if (!mText && (value & 0x4))
				{
					mText = (LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
					mText->setFont(LLFontGL::getFontSansSerif());
					mText->setVertAlignment(LLHUDText::ALIGN_VERT_TOP);
					mText->setMaxLines(-1); // Set to match current agni behavior.
					mText->setSourceObject(this);
					mText->setOnHUDAttachment(isHUDAttachment());
				}

				if (value & 0x4)
				{
					dp->unpackString(mHudTextString, "Text");
					LLColor4U coloru;
					dp->unpackBinaryDataFixed(coloru.mV, 4, "Color");
					coloru.mV[3] = 255 - coloru.mV[3];
					mHudTextColor = LLColor4(coloru);
					// Fading is disabled only when the hovetext is being
					// overridden by debug text.
					if (mText->getDoFade())
					{
						mText->setColor(mHudTextColor);
						mText->setStringUTF8(mHudTextString);
					}
//MK
					mText->mLastMessageText = mHudTextString;
//mk
					setChanged(TEXTURE);
				}
				else if (mText.notNull())
				{
					mText->markDead();
					mText = NULL;
				}

				std::string media_url;
				if (value & 0x200)
				{
					dp->unpackString(media_url, "MediaURL");
				}
				retval |= checkMediaURL(media_url);

				//
				// Unpack particle system data (legacy)
				//
				if (value & 0x8)
				{
					unpackParticleSource(*dp, owner_id, true);
				}
				else if (!(value & 0x400))
				{
					deleteParticleSource();
				}

				// Mark all extra parameters not used
				for (S32 i = 0; i < LL_EPARAMS_COUNT; ++i)
				{
					mExtraParameterInUse[i] = false;
				}

				// Unpack extra params
				U8 num_parameters;
				dp->unpackU8(num_parameters, "num_params");
				U8 param_block[MAX_OBJECT_PARAMS_SIZE];
				for (U8 param = 0; param < num_parameters; ++param)
				{
					U16 param_type;
					S32 param_size;
					dp->unpackU16(param_type, "param_type");
					dp->unpackBinaryData(param_block, param_size,
										 "param_data");
					LLDataPackerBinaryBuffer dp2(param_block, param_size);
					unpackParameterEntry(param_type, &dp2);
				}

				for (S32 i = 0; i < LL_EPARAMS_COUNT; ++i)
				{
					if (!mExtraParameterInUse[i])
					{
						parameterChanged(LL_EPARAM_TYPE(i),
										 mExtraParameters[i], false, false);
					}
				}

				if (value & 0x10)
				{
					dp->unpackUUID(sound_uuid, "SoundUUID");
					dp->unpackF32(gain, "SoundGain");
					dp->unpackU8(sound_flags, "SoundFlags");
					dp->unpackF32(cutoff, "SoundRadius");
				}

				if (value & 0x100)
				{
					std::string name_value_list;
					dp->unpackString(name_value_list, "NV");

					setNameValueList(name_value_list);
				}

				mTotalCRC = crc;
				mSoundCutOffRadius = cutoff;

				setAttachedSound(sound_uuid, owner_id, gain, sound_flags);

				// Only get these flags on updates from sim, not cached ones.
				// Preload these five flags for every object. Finer shades
				// require the object to be selected, and the selection manager
				// stores the extended permission info.
				if (mesgsys)
				{
					U32 flags;
					mesgsys->getU32Fast(_PREHASH_ObjectData,
										_PREHASH_UpdateFlags,
										flags, block_num);
					loadFlags(flags);
				}

				break;
			}

			default:
				break;
		}
	}

	//
	// Fix object parenting.
	//
	bool b_changed_status = false;

	// We only need to update parenting on full updates, terse updates
	// do not send parenting information.
	if (update_type != OUT_TERSE_IMPROVED)
	{
		U32 ip, port;
		if (mesgsys)
		{
			ip = mesgsys->getSenderIP();
			port = mesgsys->getSenderPort();
		}
		else
		{
			const LLHost& host = mRegionp->getHost();
			ip = host.getAddress();
			port = host.getPort();
		}

		LLViewerObject* sent_parentp = NULL;
		if (parent_id)
		{
			LLUUID parent_uuid;
			LLViewerObjectList::getUUIDFromLocal(parent_uuid, parent_id, ip,
												 port);
			sent_parentp = gObjectList.findObject(parent_uuid);
		}

		if (!cur_parentp)
		{
			if (parent_id)
			{
				// No parent now, new parent in message -> attach to that
				// parent if possible

				// Check to see if we have the corresponding viewer object for
				// the parent.
				if (sent_parentp && sent_parentp->getParent() == this)
				{
					// Try to recover if we attempt to attach a parent to its
					// child
					llwarns << "Attempt to attach a parent to its child: "
							<< mID << " to " << sent_parentp->mID
							<< llendl;
					removeChild(sent_parentp);
					sent_parentp->setDrawableParent(NULL);
				}

				if (sent_parentp && sent_parentp != this &&
					!sent_parentp->isDead())
				{
					// We have a viewer object for the parent, and it is not
					// dead. Do the actual reparenting here.
					b_changed_status = true;
					// ...no current parent, so do not try to remove child
					if (mDrawable.notNull())
					{
						if (mDrawable->isDead() || !mDrawable->getVObj())
						{
							llwarns << "Drawable is dead or no VObj !"
									<< llendl;
							sent_parentp->addChild(this);
						}
						else
						{
							// LLViewerObject::processUpdateMessage 1
							if (!setDrawableParent(sent_parentp->mDrawable))
							{
								// Bad, we got a cycle somehow. Kill both the
								// parent and the child, and set cache misses
								// for both of them.
								llwarns << "Attempting to recover from parenting cycle !  Killing "
										<< sent_parentp->mID << " and "
										<< mID << " and adding them to the cache miss list."
										<< llendl;
								setParent(NULL);
								sent_parentp->setParent(NULL);
								getRegion()->addCacheMissFull(getLocalID());
								getRegion()->addCacheMissFull(sent_parentp->getLocalID());
								gObjectList.killObject(sent_parentp);
								gObjectList.killObject(this);
								return retval;
							}
							sent_parentp->addChild(this);
							// Make sure this object gets a non-damped update
							if (sent_parentp->mDrawable.notNull())
							{
								gPipeline.markMoved(sent_parentp->mDrawable,
													// undamped
													false);
							}
						}
					}
					else
					{
						sent_parentp->addChild(this);
					}

					// Show particles, icon and HUD
					hideExtraDisplayItems(false);

					setChanged(MOVED | SILHOUETTE);
				}
				else
				{
					// No corresponding viewer object for the parent: put the
					// various pieces on the orphan list.
					gObjectList.orphanize(this, parent_id, ip, port);

					// Hide particles, icon and HUD
					hideExtraDisplayItems(true);
				}
			}
		}
		else
		{
			if (parent_id && !sent_parentp)
			{
				if (isAvatar())
				{
					// This logic is meant to handle the case where a
					// sitting avatar has reached a new sim ahead of the
					// object it was sitting on (which is common as objects
					// are transfered through a slower route than agents).
					// In this case, the local id for the object will not
					// be valid, since the viewer has not received a full
					// update for the object from that sim yet, so we
					// assume that the agent is still sitting where she was
					// originally. --RN
					sent_parentp = cur_parentp;
				}
				else
				{
					// Switching parents, but we do not know the new
					// parent. We are an orphan, flag things appropriately.
					gObjectList.orphanize(this, parent_id, ip, port);
				}
			}

			// Reparent if possible.
			if (sent_parentp && sent_parentp != cur_parentp &&
				sent_parentp != this)
			{
				// New parent is valid, detach and reattach
				b_changed_status = true;
				if (mDrawable.notNull())
				{
					// LLViewerObject::processUpdateMessage 2
					if (!setDrawableParent(sent_parentp->mDrawable))
					{
						// Bad, we got a cycle somehow. Kill both the parent
						// and the child, and set cache misses for both of them
						llwarns << "Attempting to recover from parenting cycle !  Killing "
								<< sent_parentp->mID << " and "
								<< mID << " and adding them to cache miss list."
								<< llendl;
						setParent(NULL);
						sent_parentp->setParent(NULL);
						getRegion()->addCacheMissFull(getLocalID());
						getRegion()->addCacheMissFull(sent_parentp->getLocalID());
						gObjectList.killObject(sent_parentp);
						gObjectList.killObject(this);
						return retval;
					}
					// Make sure this object gets a non-damped update
				}
				cur_parentp->removeChild(this);
				sent_parentp->addChild(this);
				setChanged(MOVED | SILHOUETTE);
				sent_parentp->setChanged(MOVED | SILHOUETTE);
				if (sent_parentp->mDrawable.notNull())
				{
					// false = undamped
					gPipeline.markMoved(sent_parentp->mDrawable, false);
				}
			}
			else if (!sent_parentp)
			{
				bool remove_parent = true;
				// No new parent, or the parent that we sent does not exist on
				// the viewer.
				LLViewerObject* parentp = (LLViewerObject*)getParent();
				if (parentp && parentp->getRegion() != getRegion())
				{
					// This is probably an object flying across a region
					// boundary, the object probably ISN'T being reparented,
					// but just got an object update out of order (child
					// update before parent).
					remove_parent = false;
				}

				if (remove_parent)
				{
					if (parentp && parentp == gAgentAvatarp)
					{
						LL_DEBUGS("Attachment") << "Detaching object " << mID
												<< LL_ENDL;
					}
					b_changed_status = true;
					if (mDrawable.notNull())
					{
						// Clear parent so that removeChild can put the
						// drawable on the damped list
						// LLViewerObject::processUpdateMessage 3
						setDrawableParent(NULL);
					}

					cur_parentp->removeChild(this);

					setChanged(MOVED | SILHOUETTE);

					if (mDrawable.notNull())
					{
						// Make sure this object gets a non-damped update
						gPipeline.markMoved(mDrawable, false); // undamped
					}
				}
			}
		}
	}

	new_rot.normalize();

	if (sPingInterpolate && mesgsys)
	{
		LLCircuitData* cdp =
			mesgsys->mCircuitInfo.findCircuit(mesgsys->getSender());
		if (cdp)
		{
			F32 time_dilation = mRegionp ? mRegionp->getTimeDilation() : 1.f;
			F32 ping_delay = 0.5f * time_dilation *
							 ((F32)cdp->getPingDelay() * 0.001f +
							  gFrameDT);
			LLVector3 diff = getVelocity() * ping_delay;
			new_pos_parent += diff;
		}
		else
		{
			llwarns << "findCircuit() returned NULL; skipping interpolation"
					<< llendl;
		}
	}

	//////////////////////////
	//
	// Set the generic change flags...
	//

	// If we are going to skip this message, why are we doing all the
	// parenting, etc above ?
	if (mesgsys)
	{
		U32 packet_id = mesgsys->getCurrentRecvPacketID();
		if (packet_id < mLatestRecvPacketID &&
			mLatestRecvPacketID - packet_id < 65536)
		{
			// Skip application of this message, it is old
			return retval;
		}
		mLatestRecvPacketID = packet_id;
	}

	// Set the change flags for scale
	if (new_scale != getScale())
	{
		setChanged(SCALED | SILHOUETTE);
		setScale(new_scale);  // Must follow setting permYouOwner()
	}

	// Add to mini-map objects if not yet in them and of interest; this is
	// particularly important for scripted objects which may change their
	// path-finding or physical status.
	if (!mOnMap && getPCode() == LL_PCODE_VOLUME && !isDead() &&
		(flagUsePhysics() || flagCharacter()) && !isAttachment() && isRoot())
	{
		gObjectList.addToMap(this);
		mOnMap = true;
	}

	// First, let's see if the new position is actually a change

	F32 vel_mag_sq = getVelocity().lengthSquared();
	F32 accel_mag_sq = getAcceleration().lengthSquared();

	if (b_changed_status || test_pos_parent != new_pos_parent ||
		(!mUserSelected &&
		 (vel_mag_sq != 0.f || accel_mag_sq != 0.f ||
		  this_update_precision > mBestUpdatePrecision)))
	{
		mBestUpdatePrecision = this_update_precision;

		LLVector3 diff = new_pos_parent - test_pos_parent;
		F32 mag_sqr = diff.lengthSquared();
		if (llfinite(mag_sqr))
		{
			setPositionParent(new_pos_parent);
		}
		else
		{
			llwarns << "Cannot move the object/avatar to an infinite location !"
					<< llendl;
			retval |= INVALID_UPDATE;
		}

		if (mParent && mParent->isAvatar())
		{
			// We have changed the position of an attachment, so we need to
			// clamp it
			((LLVOAvatar*)mParent)->clampAttachmentPositions();
		}
	}

	if (sUseNewTargetOmegaCode)
	{
		// New, experimental code
		bool is_new_rot = new_rot.isNotEqualEps(getRotation(), F_ALMOST_ZERO);
		if (mDebugUpdateMsg)
		{
			llinfos << "Rotation changed: " << (is_new_rot ? "yes" : "no")
					<< " - Angular velocity changed: "
					<< (new_angv != old_angv ? "yes" : "no") << llendl;
		}
		if (is_new_rot || new_angv != old_angv)
		{
			if (new_angv != old_angv)
			{
				if (new_rot != mPreviousRotation || flagUsePhysics())
				{
					resetRot();
				}
				else
				{
					mRotTime = 0.f;
				}
			}

			// Remember the last rotation value
			mPreviousRotation = new_rot;

			// Set the rotation of the object followed by adjusting for the
			// accumulated angular velocity (llSetTargetOmega)
			setRotation(new_rot * mAngularVelocityRot);
			setChanged(ROTATED | SILHOUETTE);
		}
	}
	else
	{
		// Old code
		bool is_new_rot = new_rot.isNotEqualEps(mPreviousRotation,
												F_ALMOST_ZERO);
		if (mDebugUpdateMsg)
		{
			llinfos << "Rotation changed: " << (is_new_rot ? "yes" : "no")
					<< " - Angular velocity changed: "
					<< (new_angv != old_angv ? "yes" : "no") << llendl;
		}
		if (is_new_rot || new_angv != old_angv)
		{
			if (is_new_rot)
			{
				mPreviousRotation = new_rot;
				setRotation(new_rot);
			}

			mRotTime = 0.f;
			setChanged(ROTATED | SILHOUETTE);
		}
	}

	if (gShowObjectUpdates)
	{
		LLColor4 color;
		if (update_type == OUT_TERSE_IMPROVED)
		{
			color.set(0.f, 0.f, 1.f, 1.f);
		}
		else
		{
			color.set(1.f, 0.f, 0.f, 1.f);
		}
		gPipeline.addDebugBlip(getPositionAgent(), color);
	}

	constexpr F32 MAG_CUTOFF = F_APPROXIMATELY_ZERO;
	mStatic = vel_mag_sq <= MAG_CUTOFF && accel_mag_sq <= MAG_CUTOFF &&
			  getAngularVelocity().lengthSquared() <= MAG_CUTOFF;

	// *BUG: This code leads to problems during group rotate and any scale
	// operation. Small discepencies between the simulator and viewer
	// representations cause the selection center to creep, leading to objects
	// moving around the wrong center.
	//
	// Removing this, however, means that if someone else drags an object you
	// have selected, your selection center and dialog boxes will be wrong. It
	// also means that higher precision information on selected objects will be
	// ignored.
	//
	// I believe the group rotation problem is fixed. JNC 1.21.2002

	// Additionally, if any child is selected, need to update the dialogs and
	// selection center.
	bool needs_refresh = mUserSelected;
	if (!needs_refresh)
	{
		for (child_list_t::iterator iter = mChildList.begin(),
									end = mChildList.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child && child->mUserSelected)
			{
				needs_refresh = true;
				break;
			}
		}
	}
	if (needs_refresh)
	{
		gSelectMgr.updateSelectionCenter();
		dialog_refresh_all();
	}

	// Mark update time as approx. now, with the ping delay. Ping delay is off
	// because it is not set for velocity interpolation, causing much jumping
	// and hopping around...
#if 0
	U32 ping_delay = mesgsys->mCircuitInfo.getPingDelay();
#endif
	mLastInterpUpdateSecs = LLFrameTimer::getElapsedSeconds();
	mLastMessageUpdateSecs = mLastInterpUpdateSecs;
	if (mDrawable.notNull())
	{
		// Do not clear invisibility flag on update if still orphaned !
		if (mDrawable->isState(LLDrawable::FORCE_INVISIBLE) && !mOrphaned)
		{
 			LL_DEBUGS("ViewerObject") << "Clearing force invisible: " << mID
									  << " : " << getPCodeString() << " : "
									  << getPositionAgent() << LL_ENDL;
			mDrawable->clearState(LLDrawable::FORCE_INVISIBLE);
			gPipeline.markRebuild(mDrawable);
		}
	}

	return retval;
}

// Load flags from cache or from message
void LLViewerObject::loadFlags(U32 flags)
{
	if (flags == 0xffffffff)
	{
		LL_DEBUGS("ObjectCache") << "Invalid flags for object "
								 << mID << "; ignoring." << LL_ENDL;
		return; // invalid
	}
	LL_DEBUGS("ObjectCacheSpam") << "Flags for object " << mID << " set to: "
								 << flags << LL_ENDL;

	// Keep local flags and overwrite remote-controlled flags
	mFlags = (mFlags & FLAGS_LOCAL) | flags;
	mFlagsLoaded = true;

	// ...new objects that should come in selected need to be added to the
	// selected list
	mCreateSelected = (flags & FLAGS_CREATE_SELECTED) != 0;
}

void LLViewerObject::idleUpdate(F64 time)
{
	if (!mDead)
	{
		if (!mStatic && sVelocityInterpolate && !mUserSelected)
		{
			// Calculate dt from last update
			F32 time_dilation = mRegionp ? mRegionp->getTimeDilation() : 1.f;
			F32 dt = time_dilation * (F32)(time - mLastInterpUpdateSecs);
			applyAngularVelocity(dt);

			if (isAttachment())
			{
				mLastInterpUpdateSecs = time;
				return;
			}
			else
			{
				// Move object based on its velocity and rotation
				interpolateLinearMotion(time, dt);
			}
		}

		updateDrawable(false);
	}
}

// Moves an object due to idle-time viewer side updates by iterpolating motion
void LLViewerObject::interpolateLinearMotion(F64 time, F32 dt)
{
	// Linear motion
	// PHYSICS_TIMESTEP is used below to correct for the fact that the velocity
	// in object updates represents the average velocity of the last timestep,
	// rather than the final velocity. The time dilation above should guarantee
	// that dt is never less than PHYSICS_TIMESTEP, theoretically...

	// *TODO: should also wrap linear accel/velocity in check to see if object
	// is selected, instead of explicitly zeroing it out

	F64 time_since_last_update = time - mLastMessageUpdateSecs;
	if (time_since_last_update <= 0.0 || dt <= 0.f)
	{
		return;
	}

	LLVector3 accel = getAcceleration();
	LLVector3 vel = getVelocity();

	if (sMaxUpdateInterpolationTime <= 0.0)
	{
		// Old code path... unbounded, simple interpolation.
		if (!accel.isExactlyZero() || !vel.isExactlyZero())
		{
			LLVector3 pos = (vel + 0.5f * (dt - PHYSICS_TIMESTEP) * accel) * dt;

			// Region local
			setPositionRegion(pos + getPositionRegion());
			setVelocity(vel + accel * dt);

			// For objects that are spinning but not translating, make sure to
			// flag them as having moved
			setChanged(MOVED | SILHOUETTE);
		}
	}
	else if (!accel.isExactlyZero() || !vel.isExactlyZero())
	{
		// Object is moving, and has not been too long since we got an update
		// from the server.

		// Calculate predicted position and velocity
		LLVector3 new_pos = (vel + 0.5f * (dt - PHYSICS_TIMESTEP) * accel) * dt;
		LLVector3 new_v = accel * dt;

		if (time_since_last_update > sPhaseOutUpdateInterpolationTime &&
			sPhaseOutUpdateInterpolationTime > 0.0)
		{
			// Have not seen a viewer update in a while, check to see if the
			// ciruit is still active
			if (mRegionp)
			{
				// The simulator will NOT send updates if the object continues
				// normally on the path predicted by the velocity and the
				// acceleration (often gravity) sent to the viewer. So check to
				// see if the circuit is blocked, which means the sim is likely
				// in a long lag.
				LLCircuitData* cdp =
					gMessageSystemp->mCircuitInfo.findCircuit(mRegionp->getHost());
				if (cdp)
				{
					// Find out how many seconds since last packet arrived on
					// the circuit
					F64 time_since_last_packet =
						LLMessageSystem::getMessageTimeSeconds() -
						cdp->getLastPacketInTime();
					if (!cdp->isAlive() ||	// Circuit is dead or blocked
						cdp->isBlocked() ||	// or doesn't seem to be getting any packets
						time_since_last_packet > sPhaseOutUpdateInterpolationTime)
					{
						// Start to reduce motion interpolation since we
						// haven't seen a server update in a while
						F64 time_since_last_interpolation = time - mLastInterpUpdateSecs;
						F64 phase_out = 1.0;
						if (time_since_last_update > sMaxUpdateInterpolationTime)
						{
							// Past the time limit, so stop the object
							phase_out = 0.0;
							LL_DEBUGS("MotionInterpolate") << "Motion phase out to zero"
														   << LL_ENDL;
#if 0						// Not adding this due to paranoia about stopping
							// rotation for/ TargetOmega objects and not having
							// it restart
							setAngularVelocity(LLVector3::zero);
#endif
						}
						else if (mLastInterpUpdateSecs - mLastMessageUpdateSecs >
									sPhaseOutUpdateInterpolationTime)
						{
							// Last update was already phased out a bit
							phase_out = (sMaxUpdateInterpolationTime -
										 time_since_last_update) /
										(sMaxUpdateInterpolationTime -
										 time_since_last_interpolation);
							LL_DEBUGS("MotionInterpolate") << "Continuing motion phase out of "
														   << (F32)phase_out
														   << LL_ENDL;
						}
						else
						{
							// Phase out from full value
							phase_out = (sMaxUpdateInterpolationTime -
										 time_since_last_update) /
										(sMaxUpdateInterpolationTime -
										 sPhaseOutUpdateInterpolationTime);
							LL_DEBUGS("MotionInterpolate") << "Starting motion phase out of "
														   << (F32)phase_out
														   << LL_ENDL;
						}
						phase_out = llclamp(phase_out, 0.0, 1.0);

						new_pos = new_pos * (F32)phase_out;
						new_v = new_v * (F32)phase_out;
					}
				}
			}
		}

		new_pos = new_pos + getPositionRegion();
		new_v = new_v + vel;

		// Clamp interpolated position to minimum underground and maximum
		// region height
		LLVector3d new_pos_global = mRegionp->getPosGlobalFromRegion(new_pos);
		F32 min_height;
		if (isAvatar())
		{
			// Make a better guess about AVs not going underground
			min_height = gWorld.resolveLandHeightGlobal(new_pos_global);
			min_height += 0.5f * getScale().mV[VZ];
		}
		else
		{
			// This will put the object underground, but we cannot tell if it
			// will stop at ground level or not
			min_height = gWorld.getMinAllowedZ(this, new_pos_global);
			// Cap maximum height
			new_pos.mV[VZ] = llmin(MAX_OBJECT_Z, new_pos.mV[VZ]);
		}

		new_pos.mV[VZ] = llmax(min_height, new_pos.mV[VZ]);

		// Check to see if it is going off the region
		LLVector3 temp(new_pos.mV[VX], new_pos.mV[VY], 0.f);
		// Frame time we detected region crossing in + wait time
		if (temp.clamp(0.f, mRegionp->getWidth()))
		{
			// Going off this region, so see if we might end up on another
			// region
			LLVector3d old_pos_global =
				mRegionp->getPosGlobalFromRegion(getPositionRegion());
			// Re-fetch in case it got clipped above
			new_pos_global = mRegionp->getPosGlobalFromRegion(new_pos);

			// Clip the positions to known regions
			LLVector3d clip_pos_global =
				gWorld.clipToVisibleRegions(old_pos_global, new_pos_global);
			if (clip_pos_global != new_pos_global)
			{
				// Was clipped, so this means we hit a edge where there is no
				// region to enter
				LL_DEBUGS("MotionInterpolate") << "Hit empty region edge, clipped predicted position to "
											   <<  mRegionp->getPosRegionFromGlobal(clip_pos_global)
											   << " from " << new_pos
											   << LL_ENDL;
				new_pos = mRegionp->getPosRegionFromGlobal(clip_pos_global);

				// Stop motion and get server update for bouncing on the edge
				new_v.clear();
				setAcceleration(LLVector3::zero);
			}
			else if (mRegionCrossExpire == 0.0)
			{
				// Workaround: we cannot accurately figure out time when we
				// cross border so just write down time "after the fact".
				// It is far from optimal in case of lag, but then
				// sMaxUpdateInterpolationTime will kick in first.
				LL_DEBUGS("MotionInterpolate") << "Predicted region crossing, new position"
											   << new_pos << LL_ENDL;
				mRegionCrossExpire = time + sMaxRegionCrossingInterpolationTime;
			}
			else if (time > mRegionCrossExpire)
			{
				// Predicting crossing over 1s, stop motion
				LL_DEBUGS("MotionInterpolate") << "Predicting region crossing for too long, stopping at "
											   << new_pos << LL_ENDL;
				new_v.clear();
				setAcceleration(LLVector3::zero);
				mRegionCrossExpire = 0.0;
			}
		}
		else
		{
			mRegionCrossExpire = 0.0;
		}

		// Set new position and velocity
		setPositionRegion(new_pos);
		setVelocity(new_v);

		// For objects that are spinning but not translating, make sure to flag
		// them as having moved
		setChanged(MOVED | SILHOUETTE);
	}

	// Update the last time we did anything
	mLastInterpUpdateSecs = time;
}

bool LLViewerObject::setData(const U8* datap, U32 data_size)
{
	delete[] mData;

	if (datap)
	{
		mData = new U8[data_size];
		if (!mData)
		{
			return false;
		}
		memcpy(mData, datap, data_size);
	}
	return true;
}

// Delete an item in the inventory, but do not tell the server. This is used
// internally by remove, update, and savescript. This will only delete the
// first item with an item_id in the list.
void LLViewerObject::deleteInventoryItem(const LLUUID& item_id)
{
	if (mInventory)
	{
		for (LLInventoryObject::object_list_t::iterator
				it = mInventory->begin(), end = mInventory->end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = *it;
			if (obj && obj->getUUID() == item_id)
			{
				// This is safe only because we return immediatly.
				mInventory->erase(it); // will deref and delete it
				return;
			}
		}
		doInventoryCallback();
	}
}

void LLViewerObject::doUpdateInventory(LLPointer<LLViewerInventoryItem>& itemp,
									   bool is_new)
{
	if (is_new)
	{
		++mExpectedInventorySerialNum;
		return;
	}
	if (!mInventory)
	{
		return;
	}

	LLUUID item_id;
	LLUUID new_owner;
	LLUUID new_group;
	bool group_owned = false;
	LLViewerInventoryItem* old_itemp =
		(LLViewerInventoryItem*)getInventoryObject(itemp->getUUID());
	if (old_itemp)
	{
		item_id = old_itemp->getUUID();
		new_owner = old_itemp->getPermissions().getOwner();
		new_group = old_itemp->getPermissions().getGroup();
		group_owned = old_itemp->getPermissions().isGroupOwned();
	}
	else
	{
		item_id = itemp->getUUID();
	}

	// Attempt to update the local inventory. If we can get the object
	// permissions, we have perfect visibility, so we want the serial number to
	// match. Otherwise, take our best guess and make sure that the serial
	// number does not match.
	deleteInventoryItem(item_id);
	LLPermissions perm(itemp->getPermissions());
	LLPermissions* obj_permp = gSelectMgr.findObjectPermissions(this);
	bool is_atomic = itemp->getType() != (S32)LLAssetType::AT_OBJECT;
	if (obj_permp)
	{
		perm.setOwnerAndGroup(LLUUID::null, obj_permp->getOwner(),
							  obj_permp->getGroup(), is_atomic);
	}
	else if (group_owned)
	{
		perm.setOwnerAndGroup(LLUUID::null, new_owner, new_group, is_atomic);
	}
	else if (new_owner.notNull())
	{
		// The object used to be in inventory, so we can assume the owner and
		// group will match what they are there.
		perm.setOwnerAndGroup(LLUUID::null, new_owner, new_group, is_atomic);
	}
	// *FIXME: could make an even better guess by using the mPermGroup flags
	else if (permYouOwner())
	{
		// Best guess.
		perm.setOwnerAndGroup(LLUUID::null, gAgentID,
							  itemp->getPermissions().getGroup(), is_atomic);
		--mExpectedInventorySerialNum;
	}
	else
	{
		// Dummy it up.
		perm.setOwnerAndGroup(LLUUID::null, LLUUID::null, LLUUID::null,
							  is_atomic);
		--mExpectedInventorySerialNum;
	}

	LLViewerInventoryItem* new_itemp = new LLViewerInventoryItem(itemp.get());
	new_itemp->setPermissions(perm);
	mInventory->emplace_front(new_itemp);
	doInventoryCallback();
	++mExpectedInventorySerialNum;
}

// Saves a script, which involves removing the old one, and rezzing in the new
// one. This method should be called with the asset id of the new and old
// script AFTER the bytecode has been saved.
void LLViewerObject::saveScript(const LLViewerInventoryItem* item, bool active,
								bool is_new)
{
	/*
	 * XXXPAM Investigate not making this copy. Seems unecessary, but I am
	 * unsure about the interaction with doUpdateInventory() called below.
	 */
	LL_DEBUGS("ViewerObject") << "Saving script for object: " << mID
							  << ". Inventory item Id: " << item->getUUID()
							  << ". Asset Id: " << item->getAssetUUID()
							  << LL_ENDL;
	LLPointer<LLViewerInventoryItem> task_item =
		new LLViewerInventoryItem(item->getUUID(), mID, item->getPermissions(),
								  item->getAssetUUID(), item->getType(),
								  item->getInventoryType(),
								  item->getName(), item->getDescription(),
								  item->getSaleInfo(), item->getFlags(),
								  item->getCreationDate());
	task_item->setTransactionID(item->getTransactionID());

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RezScript);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
	msg->nextBlockFast(_PREHASH_UpdateBlock);
	msg->addU32Fast(_PREHASH_ObjectLocalID, mLocalID);
	msg->addBoolFast(_PREHASH_Enabled, active);
	msg->nextBlockFast(_PREHASH_InventoryBlock);
	task_item->packMessage(msg);
	msg->sendReliable(mRegionp->getHost());

	// Do the internal logic
	doUpdateInventory(task_item, is_new);
}

void LLViewerObject::moveInventory(const LLUUID& folder_id,
								   const LLUUID& item_id)
{
	LL_DEBUGS("ViewerObject") << "Moving inventory item " << item_id
							  << LL_ENDL;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MoveTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_FolderID, folder_id);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addU32Fast(_PREHASH_LocalID, mLocalID);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->sendReliable(mRegionp->getHost());

	LLInventoryObject* inv_obj = getInventoryObject(item_id);
	if (inv_obj)
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)inv_obj;
		if (!item->getPermissions().allowCopyBy(gAgentID))
		{
			deleteInventoryItem(item_id);
			++mExpectedInventorySerialNum;
		}
	}
}

void LLViewerObject::dirtyInventory()
{
	// If there is no LLVOInventoryListener, we will not be able to update our
	// mInventory when it comes back from the simulator, so we should not clear
	// the inventory either.
	if (mInventory && !mInventoryCallbacks.empty())
	{
		mInventory->clear(); // will deref and delete entries
		delete mInventory;
		mInventory = NULL;
	}
	mInventoryDirty = true;
}

void LLViewerObject::registerInventoryListener(LLVOInventoryListener* listener,
											   void* user_data)
{
	LLInventoryCallbackInfo* info = new LLInventoryCallbackInfo;
	info->mObject = this;
	info->mListener = listener;
	info->mInventoryData = user_data;
	mInventoryCallbacks.push_front(info);
}

void LLViewerObject::removeInventoryListener(LLVOInventoryListener* listener)
{
	if (!listener) return;

	for (callback_list_t::iterator iter = mInventoryCallbacks.begin();
		 iter != mInventoryCallbacks.end(); )
	{
		callback_list_t::iterator curiter = iter++;
		LLInventoryCallbackInfo* info = *curiter;
		if (info && info->mListener == listener)
		{
			delete info;
			mInventoryCallbacks.erase(curiter);
			break;
		}
	}
}

void LLViewerObject::clearInventoryListeners()
{
	for_each(mInventoryCallbacks.begin(), mInventoryCallbacks.end(),
			 DeletePointer());
	mInventoryCallbacks.clear();
}

void LLViewerObject::requestInventory()
{
	if (mInventoryDirty && mInventory && !mInventoryCallbacks.empty())
	{
		mInventory->clear(); // will deref and delete it
		delete mInventory;
		mInventory = NULL;
	}
	if (mInventory)
	{
		// Inventory is either up to date or does not have a listener if it is
		// dirty, leave it this way in case we gain a listener.
		doInventoryCallback();
	}
	else
	{
		// Since we are going to request it now.
		mInventoryDirty = false;

		// Throw away duplicate requests.
		fetchInventoryFromServer();
	}
}

void LLViewerObject::fetchInventoryFromServer()
{
	if (!isInventoryPending())
	{
		delete mInventory;
		mInventory = NULL;

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_RequestTaskInventory);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addU32Fast(_PREHASH_LocalID, mLocalID);
		msg->sendReliable(mRegionp->getHost());

		// this will get reset by dirtyInventory or doInventoryCallback
		mInvRequestState = INVENTORY_REQUEST_PENDING;
	}
}

LLVOAvatarPuppet* LLViewerObject::getPuppetAvatar()
{
	return getRootEdit()->mPuppetAvatar.get();
}

LLVOAvatarPuppet* LLViewerObject::getPuppetAvatar() const
{
	return getRootEdit()->mPuppetAvatar.get();
}

void LLViewerObject::linkPuppetAvatar()
{
	LLVOAvatarPuppet* puppet = getPuppetAvatar();
	if (!puppet && isRootEdit())
	{
		LLVOVolume* volp = asVolume();
		if (!volp)
		{
			llwarns << "Called with null or non-volume object" << llendl;
			return;
		}
		puppet = mPuppetAvatar = LLVOAvatarPuppet::createAvatarPuppet(volp);
	}
	if (puppet)
	{
		puppet->updateAttachmentOverrides();
		if (!puppet->mPlaying)
		{
			puppet->mPlaying = true;
#if 0
			if (!puppet->mRootVolp->isAnySelected())
#endif
			{
				puppet->updateVolumeGeom();
				puppet->mRootVolp->recursiveMarkForUpdate();
			}
		}
	}
	else
	{
		llwarns << "No puppet avatar found !" << llendl;
	}
}

void LLViewerObject::unlinkPuppetAvatar()
{
	LLVOAvatarPuppet* puppet = getPuppetAvatar();
	if (puppet)
	{
		puppet->updateAttachmentOverrides();
		if (isRootEdit())
		{
			// This will remove the entire linkset from the puppet avatar.
			// Note: mPuppetAvatar == puppet for root edit, so it is not NULL
			// here.
			mPuppetAvatar->markForDeath();
			mPuppetAvatar = NULL;
		}
		// For non-root prims, removing from the linkset will automatically
		// remove the puppet avatar connection.
	}
}

void LLViewerObject::updatePuppetAvatar()
{
	LLViewerObject* root = getRootEdit();
	bool animated = root->isAnimatedObject();
	LLVOAvatarPuppet* puppet = root->mPuppetAvatar.get();
	bool no_puppet = puppet == NULL;
	if (no_puppet && !animated)
	{
		return;
	}

	bool is_rigged_mesh = false;
	if (animated)
	{
		is_rigged_mesh = root->isRiggedMesh();
		if (!is_rigged_mesh)
		{
			const_child_list_t& child_list = root->getChildren();
			for (const_child_list_t::const_iterator iter = child_list.begin(),
													end = child_list.end();
				 iter != end; ++iter)
			{
				const LLViewerObject* child = *iter;
				if (child && child->isRiggedMesh())
				{
					is_rigged_mesh = true;
					break;
				}
			}
		}
	}

	if (animated && is_rigged_mesh)
	{
		if (no_puppet)
		{
			root->linkPuppetAvatar();
		}
	}
	else if (!no_puppet)
	{
		root->unlinkPuppetAvatar();
	}

	if (puppet)
	{
		puppet->updateAnimations();
		if (mUserSelected)
		{
			gSelectMgr.pauseAssociatedAvatars();
		}
#if LL_ANIMESH_VPARAMS
		// *FIXME: Axon: this should work for visual params of animated
		// objects, but it is less clear where this function should be called
		// in general. Need to handle the case of object getting constructed
		// sometime after the extended attributes have arrived, for arbitrary
		// object type.
		applyExtendedAttributes();
#endif
	}
}

struct LLFilenameAndTask
{
	LL_INLINE LLFilenameAndTask(const LLUUID& id, const std::string& fname,
								S16 serial)
	:	mTaskID(id),
		mFilename(fname),
		mSerial(serial)
	{
	}

	LLUUID		mTaskID;
	std::string	mFilename;
	S16			mSerial;	// For sequencing in case of multiple updates
};

//static
void LLViewerObject::processTaskInv(LLMessageSystem* msg, void**)
{
	LLUUID task_id;
	msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_TaskID, task_id);
	LLViewerObject* object = gObjectList.findObject(task_id);
	if (!object)
	{
		llwarns << "Object " << task_id << " does not exist." << llendl;
		return;
	}

	// Note: we can receive multiple task updates simultaneously, make sure we
	// will not rewrite newer with older update
	S16 serial;
	msg->getS16Fast(_PREHASH_InventoryData, _PREHASH_Serial, serial);
	if (serial == object->mInventorySerialNum &&
		serial < object->mExpectedInventorySerialNum)
	{
		// Loop protection. We received same serial twice.  Viewer did some
		// changes to inventory that could not be saved yet or something went
		// wrong to cause serial to be out of sync...
		// ... but only warn if inventory has already been received once (else
		// it simply means we are still waiting for initial data: seen while
		// inspecting avatars attachments).
		if (serial)
		{
			llwarns << "Task inventory serial might be out of sync, server serial: "
					<< serial << " - Client expected serial: "
					<< object->mExpectedInventorySerialNum << llendl;
		}
		object->mExpectedInventorySerialNum = serial;
	}

	if (serial < object->mExpectedInventorySerialNum)
	{
		// Out of date message; record to current serial for loop protection,
		// but do not load it: just drop xfer to restart on idle.
		if (serial < object->mInventorySerialNum)
		{
			llwarns << "Task inventory serial has decreased: out of order packet ?  Server serial: "
				<< serial << " - Client expected serial: "
				<< object->mExpectedInventorySerialNum << llendl;
		}
		object->mInventorySerialNum = serial;
		object->mInvRequestXFerId = 0;
		object->mInvRequestState = INVENTORY_REQUEST_STOPPED;
		return;
	}

	object->mInventorySerialNum = object->mExpectedInventorySerialNum = serial;

	std::string filename;
	msg->getStringFast(_PREHASH_InventoryData, _PREHASH_Filename, filename);
	filename = LLDir::getScrubbedFileName(filename);
	if (filename.empty())
	{
		LL_DEBUGS("ViewerObject") << "Task has no inventory" << LL_ENDL;
		// Mock up some inventory to make a drop target.
		if (object->mInventory)
		{
			object->mInventory->clear(); // Will deref and delete it
		}
		else
		{
			object->mInventory = new LLInventoryObject::object_list_t();
		}
		object->mInventory->emplace_front(new LLInventoryObject(object->mID,
																LLUUID::null,
																LLAssetType::AT_CATEGORY,
																"Contents"));
		object->doInventoryCallback();
		return;
	}

	if (!gXferManagerp)
	{
		llwarns << "Transfer manager gone. Aborted." << llendl;
		return;
	}

	LLFilenameAndTask* ft = new LLFilenameAndTask(task_id, filename, serial);
	U64 new_id =
		gXferManagerp->requestFile(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
																  filename),
								   filename, LL_PATH_CACHE,
								   object->mRegionp->getHost(), true,
								   processTaskInvFile, (void**)ft,
								   LLXferManager::HIGH_PRIORITY);
	if (object->mInvRequestState == INVENTORY_XFER)
	{
		if (new_id && new_id != object->mInvRequestXFerId)
		{
			// We started a new download. Abort the old one.
			gXferManagerp->abortRequestById(object->mInvRequestXFerId, -1);
			object->mInvRequestXFerId = new_id;
		}
	}
	else
	{
		object->mInvRequestState = INVENTORY_XFER;
		object->mInvRequestXFerId = new_id;
	}
}

void LLViewerObject::processTaskInvFile(void** user_data, S32 error_code,
										LLExtStat ext_status)
{
	LLFilenameAndTask* ft = (LLFilenameAndTask*)user_data;
	LLViewerObject* objectp;
	if (ft && error_code == 0 &&
		(objectp = gObjectList.findObject(ft->mTaskID)) &&
		ft->mSerial >= objectp->mInventorySerialNum)
	{
		objectp->mInventorySerialNum = ft->mSerial;
		LL_DEBUGS("ViewerObject") << "Receiving inventory task file for serial: "
								  << objectp->mInventorySerialNum
								  << " - Expected serial: "
								  << objectp->mExpectedInventorySerialNum
								  << " - Task Id: " << ft->mTaskID << LL_ENDL;
		if (objectp->loadTaskInvFile(ft->mFilename))
		{
			uuid_list_t& pending = objectp->mPendingInventoryItemsIDs;
			for (LLInventoryObject::object_list_t::iterator
					it = objectp->mInventory->begin(),
					end = objectp->mInventory->end();
				 it != end && !pending.empty(); ++it)
			{
				LLViewerInventoryItem* itemp =
					it->get()->asViewerInventoryItem();
				if (itemp && itemp->getType() != LLAssetType::AT_CATEGORY)
				{
					// Erase if present. No-op when absent.
					pending.erase(itemp->getAssetUUID());
				}
			}
		}
	}
	else
	{
		// This occurs when two requests were made, and the first one has
		// already handled it.
		LL_DEBUGS("ViewerObject") << "Problem loading task inventory. Return code: "
								  << error_code << LL_ENDL;
	}
	if (ft)
	{
		delete ft;
	}
}

bool LLViewerObject::loadTaskInvFile(const std::string& filename)
{
	std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														   filename);
	llifstream ifs(full_path.c_str());
	if (!ifs.good())
	{
		llwarns << "Unable to load task inventory: " << full_path << llendl;
		return false;
	}

	if (mInventory)
	{
		mInventory->clear(); // Will deref and delete it
	}
	else
	{
		mInventory = new LLInventoryObject::object_list_t;
	}

	char buffer[MAX_STRING];
	// *NOTE: This buffer size is hard coded into scanf() below.
	char keyword[MAX_STRING];
	U32 failed = 0;
	while (ifs.good())
	{
		ifs.getline(buffer, MAX_STRING);
		if (sscanf(buffer, " %254s", keyword) < 1)
		{
			continue;	// Ignore empty lines. HB
		}
		if (strcmp("inv_item", keyword) == 0)
		{
			LLPointer<LLInventoryObject> inv_objp = new LLViewerInventoryItem;
			inv_objp->importLegacyStream(ifs);
			mInventory->emplace_front(std::move(inv_objp));
		}
		else if (strcmp("inv_object", keyword) == 0)
		{
			LLPointer<LLInventoryObject> inv_objp = new LLInventoryObject;
			inv_objp->importLegacyStream(ifs);
			mInventory->emplace_front(std::move(inv_objp));
		}
		else if (++failed > MAX_INV_FILE_READ_FAILS)
		{
			llwarns << "Too many unknown token in inventory file: " << filename
					<< ". Aborting." << llendl;
			break;
		}
		else
		{
			llwarns << "Unknown token '" << keyword << "' in inventory file: "
					<< filename << llendl;
		}
	}
	ifs.close();

	LLFile::remove(full_path);

	doInventoryCallback();

	return true;
}

void LLViewerObject::doInventoryCallback()
{
	for (callback_list_t::iterator iter = mInventoryCallbacks.begin();
		 iter != mInventoryCallbacks.end(); )
	{
		callback_list_t::iterator curiter = iter++;
		LLInventoryCallbackInfo* info = *curiter;
		if (info && info->mListener)
		{
			info->mListener->inventoryChanged(this, mInventory,
											  mInventorySerialNum,
											  info->mInventoryData);
		}
		else
		{
			llinfos << "Deleting bad listener entry." << llendl;
			delete info;
			mInventoryCallbacks.erase(curiter);
		}
	}

	// Release inventory loading state
	mInvRequestXFerId = 0;
	mInvRequestState = INVENTORY_REQUEST_STOPPED;
}

void LLViewerObject::removeInventory(const LLUUID& item_id)
{
	// close any associated floater properties
	LLFloaterProperties::closeByID(item_id, mID);

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RemoveTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addU32Fast(_PREHASH_LocalID, mLocalID);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->sendReliable(mRegionp->getHost());
	deleteInventoryItem(item_id);
	++mExpectedInventorySerialNum;
}

bool LLViewerObject::isAssetInInventory(LLViewerInventoryItem* itemp,
										LLAssetType::EType type)
{
	if (!itemp)
	{
		return false;
	}
	// Note: for now mPendingInventoryItemsIDs only stores textures and
	// GLTF materials, but if it gets to store more types, it will need to
	// verify type as well since null can be a shared default UUID and it is
	// fine to need a null script and a null material simultaneously.
	const LLUUID& asset_id = itemp->getAssetUUID();
	return mPendingInventoryItemsIDs.count(asset_id) > 0 ||
		   getInventoryItemByAsset(asset_id, type) != NULL;
}

void LLViewerObject::updateInventory(LLViewerInventoryItem* itemp, bool is_new)
{
	if (!itemp)	// Paranoia
	{
		return;
	}

	if (is_new)
	{
		LLAssetType::EType type = itemp->getType();
		if (type == LLAssetType::AT_TEXTURE ||
			type == LLAssetType::AT_MATERIAL)
		{
			if (isAssetInInventory(itemp, type))
			{
				return;	// Already here
			}
			mPendingInventoryItemsIDs.emplace(itemp->getAssetUUID());
		}
	}

	// This slices the object into what we are concerned about on the viewer.
	// The simulator will take the permissions and transfer ownership.
	LLPointer<LLViewerInventoryItem> task_itemp =
		new LLViewerInventoryItem(itemp->getUUID(), mID,
								  itemp->getPermissions(),
								  itemp->getAssetUUID(), itemp->getType(),
								  itemp->getInventoryType(), itemp->getName(),
								  itemp->getDescription(),
								  itemp->getSaleInfo(), itemp->getFlags(),
								  itemp->getCreationDate());
	task_itemp->setTransactionID(itemp->getTransactionID());
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_UpdateTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_UpdateData);
	msg->addU32Fast(_PREHASH_LocalID, mLocalID);
	msg->addU8Fast(_PREHASH_Key, TASK_INVENTORY_ITEM_KEY);
	msg->nextBlockFast(_PREHASH_InventoryData);
	task_itemp->packMessage(msg);
	msg->sendReliable(mRegionp->getHost());

	// Do the internal logic
	doUpdateInventory(task_itemp, is_new);
}

LLInventoryObject* LLViewerObject::getInventoryObject(const LLUUID& item_id)
{
	LLInventoryObject* rv = NULL;
	if (mInventory && item_id.notNull())
	{
		for (LLInventoryObject::object_list_t::const_iterator
				it = mInventory->begin(), end = mInventory->end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = *it;
			if (obj && obj->getUUID() == item_id)
			{
				rv = *it;
				break;
			}
		}
	}
	return rv;
}

LLInventoryItem* LLViewerObject::getInventoryItem(const LLUUID& item_id)
{
	LLInventoryObject* invobjp = getInventoryObject(item_id);
	if (invobjp && invobjp->getType() != LLAssetType::AT_CATEGORY)
	{
		return dynamic_cast<LLInventoryItem*>(invobjp);
	}
	return NULL;
}

void LLViewerObject::getInventoryContents(LLInventoryObject::object_list_t& objects)
{
	if (mInventory)
	{
		for (LLInventoryObject::object_list_t::const_iterator
				it = mInventory->begin(), end = mInventory->end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = *it;
			if (obj && obj->getType() != LLAssetType::AT_CATEGORY)
			{
				objects.push_back(obj);
			}
		}
	}
}

LLInventoryObject* LLViewerObject::getInventoryRoot()
{
	return mInventory && !mInventory->empty() ? mInventory->back().get()
											  : NULL;
}

LLViewerInventoryItem* LLViewerObject::getInventoryItemByAsset(const LLUUID& asset_id,
															   LLAssetType::EType type)
{
	if (mInventoryDirty)
	{
		llwarns << "Performing inventory lookup for object " << mID
				<< " that has dirty inventory !" << llendl;
	}
	if (type == LLAssetType::AT_CATEGORY)
	{
		llwarns << "Attempted to get an inventory asset for category Id: "
				<< asset_id << llendl;
		// Whatever called this should not be trying to get a folder by asset,
		// since categories do not have any asset !
		llassert(false);
		return NULL;
	}
	if (mInventory)
	{
		for (LLInventoryObject::object_list_t::const_iterator
				it = mInventory->begin(), end = mInventory->end();
			 it != end; ++it)
		{
			LLInventoryObject* objp = *it;
			if (!objp) continue;
			LLAssetType::EType atype = objp->getType();
			if (atype != LLAssetType::AT_CATEGORY &&
				atype != LLAssetType::AT_NONE)
			{
				LLViewerInventoryItem* itemp = objp->asViewerInventoryItem();
				if (itemp && itemp->getAssetUUID() == asset_id)
				{
					if (type == LLAssetType::AT_NONE || type == atype)
					{
						return itemp;
					}
				}
			}
		}
	}
	return NULL;
}

void LLViewerObject::updateViewerInventoryAsset(const LLViewerInventoryItem* item,
												const LLUUID& new_asset)
{
	LLPointer<LLViewerInventoryItem> task_item =
		new LLViewerInventoryItem(item);
	task_item->setAssetUUID(new_asset);

	// Do the internal logic
	doUpdateInventory(task_item, false);
}

void LLViewerObject::setPixelAreaAndAngle()
{
	if (getVolume())
	{
		// Volumes calculate pixel area and angle per face
		return;
	}

	LLVector3 viewer_pos_agent = gAgent.getCameraPositionAgent();
	LLVector3 pos_agent = getRenderPosition();

	F32 dx = viewer_pos_agent.mV[VX] - pos_agent.mV[VX];
	F32 dy = viewer_pos_agent.mV[VY] - pos_agent.mV[VY];
	F32 dz = viewer_pos_agent.mV[VZ] - pos_agent.mV[VZ];

	F32 max_scale = getMaxScale();
	F32 mid_scale = getMidScale();
	F32 min_scale = getMinScale();

	// IW: estimate, when close to large objects, computing range based on
	// distance from center is no good to try to get a min distance from face,
	// subtract min_scale/2 from the range. This means we will load too much
	// detail sometimes, but that is better than not enough. I do not think
	// there is a better way to do this without calculating distance per-poly.
	F32 range = sqrtf(dx * dx + dy * dy + dz * dz) - 0.5f * min_scale;
	if (range < 0.001f || isHUDAttachment())		// range == zero
	{
		mAppAngle = 180.f;
		mPixelArea = (F32)gViewerCamera.getScreenPixelArea();
	}
	else
	{
		mAppAngle = atan2f(max_scale, range) * RAD_TO_DEG;

		F32 pixels_per_meter = gViewerCamera.getPixelMeterRatio() / range;

		mPixelArea = (pixels_per_meter * max_scale) *
					 (pixels_per_meter * mid_scale);
		if (mPixelArea > gViewerCamera.getScreenPixelArea())
		{
			mAppAngle = 180.f;
			mPixelArea = (F32)gViewerCamera.getScreenPixelArea();
		}
	}
}

void LLViewerObject::setScale(const LLVector3& scale, bool damped)
{
	LLPrimitive::setScale(scale);
	if (mDrawable.notNull())
	{
		// Encompass completely sheared objects by taking the most extreme
		// point possible (<1,1,0.5>)
		mDrawable->setRadius(LLVector3(1.f, 1.f, 0.5f).scaleVec(scale).length());
		updateDrawable(damped);
	}

	if (getPCode() == LL_PCODE_VOLUME && !isDead() && !isAttachment())
	{
		bool plottable = (flagCharacter() || flagUsePhysics()) && isRoot();
		if (plottable || permYouOwner() || scale.lengthSquared() > 7.5f * 7.5f)
		{
			if (!mOnMap)
			{
				gObjectList.addToMap(this);
				mOnMap = true;
			}
		}
		else if (mOnMap)
		{
			gObjectList.removeFromMap(this);
			mOnMap = false;
		}
	}
}

void LLViewerObject::setObjectCost(F32 cost)
{
	mObjectCost = cost;
	mCostStale = false;

	if (gFloaterToolsp && mUserSelected)
	{
		gFloaterToolsp->dirty();
	}
}

void LLViewerObject::setLinksetCost(F32 cost)
{
	mLinksetCost = cost;
	mCostStale = false;

	if (gFloaterToolsp && mUserSelected)
	{
		gFloaterToolsp->dirty();
	}
}

void LLViewerObject::setPhysicsCost(F32 cost)
{
	mPhysicsCost = cost;
	mCostStale = false;

	if (gFloaterToolsp && mUserSelected)
	{
		gFloaterToolsp->dirty();
	}
}

void LLViewerObject::setLinksetPhysicsCost(F32 cost)
{
	mLinksetPhysicsCost = cost;
	mCostStale = false;

	if (gFloaterToolsp && mUserSelected)
	{
		gFloaterToolsp->dirty();
	}
}

F32 LLViewerObject::getObjectCost()
{
	if (mCostStale)
	{
		gObjectList.updateObjectCost(this);
	}

	return mObjectCost;
}

F32 LLViewerObject::getLinksetCost()
{
	if (mCostStale)
	{
		gObjectList.updateObjectCost(this);
	}

	return mLinksetCost;
}

F32 LLViewerObject::getPhysicsCost()
{
	if (mCostStale)
	{
		gObjectList.updateObjectCost(this);
	}

	return mPhysicsCost;
}

F32 LLViewerObject::getLinksetPhysicsCost()
{
	if (mCostStale)
	{
		gObjectList.updateObjectCost(this);
	}

	return mLinksetPhysicsCost;
}

F32 LLViewerObject::recursiveGetEstTrianglesMax() const
{
	F32 est_tris = getEstTrianglesMax();
	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* childp = *iter;
		if (childp && !childp->isAvatar())
		{
			est_tris += childp->recursiveGetEstTrianglesMax();
		}
	}
	return est_tris;
}

U32 LLViewerObject::recursiveGetTriangleCount(S32* vcount) const
{
	S32 total_tris = getTriangleCount(vcount);
	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* childp = *iter;
		if (childp)
		{
			total_tris += childp->getTriangleCount(vcount);
		}
	}
	return total_tris;
}

S32 LLViewerObject::getAnimatedObjectMaxTris() const
{
	S32 res = 0;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		const LLSD& info = regionp->getSimulatorFeatures();
		if (info.has("AnimatedObjects"))
		{
			res = info["AnimatedObjects"]["AnimatedObjectMaxTris"].asInteger();
		}
	}

	return res;
}

void LLViewerObject::updateSpatialExtents(LLVector4a& new_min,
										  LLVector4a& new_max)
{
	if (mDrawable.notNull())
	{
		LLVector4a center;
		center.load3(getRenderPosition().mV);
		LLVector4a size;
		size.load3(getScale().mV);
		new_min.setSub(center, size);
		new_max.setAdd(center, size);

		mDrawable->setPositionGroup(center);
	}
	else
	{
		llwarns << "Call done for an object with NULL mDrawable" << llendl;
	}
}

F32 LLViewerObject::getBinRadius()
{
	if (mDrawable.notNull())
	{
		const LLVector4a* ext = mDrawable->getSpatialExtents();
		LLVector4a diff;
		diff.setSub(ext[1], ext[0]);
		return diff.getLength3().getF32();
	}

	return getScale().length();
}

F32 LLViewerObject::getMaxScale() const
{
	return llmax(getScale().mV[VX],getScale().mV[VY], getScale().mV[VZ]);
}

F32 LLViewerObject::getMinScale() const
{
	return llmin(getScale().mV[0],getScale().mV[1],getScale().mV[2]);
}

F32 LLViewerObject::getMidScale() const
{
	if (getScale().mV[VX] < getScale().mV[VY])
	{
		if (getScale().mV[VY] < getScale().mV[VZ])
		{
			return getScale().mV[VY];
		}
		else if (getScale().mV[VX] < getScale().mV[VZ])
		{
			return getScale().mV[VZ];
		}
		else
		{
			return getScale().mV[VX];
		}
	}
	else if (getScale().mV[VX] < getScale().mV[VZ])
	{
		return getScale().mV[VX];
	}
	else if (getScale().mV[VY] < getScale().mV[VZ])
	{
		return getScale().mV[VZ];
	}
	else
	{
		return getScale().mV[VY];
	}
}

void LLViewerObject::boostTexturePriority(bool boost_children)
{
	if (isDead())
	{
		return;
	}

	for (S32 i = 0, count = getNumTEs(); i < count; ++i)
	{
		LLViewerTexture* texp = getTEImage(i);
		if (texp)
		{
	 		texp->setBoostLevel(LLGLTexture::BOOST_SELECTED);
		}
	}

	if (isSculpted() && !isMesh())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		const LLUUID& sculpt_id = sculpt_params->getSculptTexture();
		LLViewerTextureManager::getFetchedTexture(sculpt_id, FTT_DEFAULT, true,
												  LLGLTexture::BOOST_NONE,
												  LLViewerTexture::LOD_TEXTURE)->setBoostLevel(LLGLTexture::BOOST_SELECTED);
	}

	if (boost_children)
	{
		for (child_list_t::iterator iter = mChildList.begin(),
									end = mChildList.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child)	// Paranoia
			{
				child->boostTexturePriority();
			}
		}
	}
}

void LLViewerObject::setLineWidthForWindowSize(S32 window_width)
{
	if (window_width < 700)
	{
		LLUI::setLineWidth(2.f);
	}
	else if (window_width < 1100)
	{
		LLUI::setLineWidth(3.f);
	}
	else if (window_width < 2000)
	{
		LLUI::setLineWidth(4.f);
	}
	else
	{
		LLUI::setLineWidth(5.f);
	}
}

// Culled from newsim LLTask::addNVPair
void LLViewerObject::addNVPair(const std::string& data)
{
	LLNameValue* nv = new LLNameValue(data.c_str());
	name_value_map_t::iterator iter = mNameValuePairs.find(nv->mName);
	if (iter != mNameValuePairs.end())
	{
		LLNameValue* foundnv = iter->second;
		if (foundnv->mClass != NVC_READ_ONLY)
		{
			delete foundnv;
			mNameValuePairs.erase(iter);
		}
		else
		{
			delete nv;
			return;
		}
	}
	mNameValuePairs[nv->mName] = nv;
}

bool LLViewerObject::removeNVPair(const std::string& name)
{
	char* canonical_name = gNVNameTable.addString(name);

	LL_DEBUGS("ViewerObject") << "Removing: " << name << LL_ENDL;

	name_value_map_t::iterator iter = mNameValuePairs.find(canonical_name);
	if (iter != mNameValuePairs.end())
	{
		if (mRegionp)
		{
			LLNameValue* nv = iter->second;
#if 0
			std::string buffer = nv->printNameValue();
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_RemoveNameValuePair);
			msg->nextBlockFast(_PREHASH_TaskData);
			msg->addUUIDFast(_PREHASH_ID, mID);

			msg->nextBlockFast(_PREHASH_NameValueData);
			msg->addStringFast(_PREHASH_NVPair, buffer);

			msg->sendReliable(mRegionp->getHost());
#endif
			// Remove the NV pair from the local list.
			delete nv;
			mNameValuePairs.erase(iter);

			return true;
		}
		else
		{
			LL_DEBUGS("ViewerObject") << "No region for object" << LL_ENDL;
		}
	}
	return false;
}

LLNameValue* LLViewerObject::getNVPair(const std::string& name) const
{
	char* canonical_name = gNVNameTable.addString(name);
	if (!canonical_name)
	{
		return NULL;
	}

	// If you access a map with a name that is not in it, it will add the name
	// and a null pointer. So first check if the data is in the map.
	name_value_map_t::const_iterator it = mNameValuePairs.find(canonical_name);
	if (it != mNameValuePairs.end())
	{
		return it->second;
	}
	return NULL;
}

void LLViewerObject::updatePositionCaches() const
{
	// If region is removed from the list it is also deleted.
	if (mRegionp && gWorld.isRegionListed(mRegionp))
	{
		if (!isRoot())
		{
			LLViewerObject* parent = (LLViewerObject*)getParent();
			if (!parent)
			{
				llwarns_once << "No parent for child object " << mID << llendl;
				llassert(false);
				return;
			}
			mPositionRegion = parent->getPositionRegion() +
							  getPosition() * parent->getRotation();
			mPositionAgent = mRegionp->getPosAgentFromRegion(mPositionRegion);
		}
		else
		{
			mPositionRegion = getPosition();
			mPositionAgent = mRegionp->getPosAgentFromRegion(mPositionRegion);
		}
	}
}

const LLVector3d LLViewerObject::getPositionGlobal() const
{
	LLVector3d pos_global;

	// If region is removed from the list it is also deleted.
	if (mRegionp && gWorld.isRegionListed(mRegionp))
	{
		if (isAttachment())
		{
			pos_global = gAgent.getPosGlobalFromAgent(getRenderPosition());
		}
		else
		{
			pos_global = mRegionp->getPosGlobalFromRegion(getPositionRegion());
		}
	}
	else
	{
		pos_global.set(getPosition());
	}

	return pos_global;
}

const LLVector3& LLViewerObject::getPositionAgent() const
{
	// If region is removed from the list it is also deleted.
	if (mRegionp && gWorld.isRegionListed(mRegionp))
	{
		if (mDrawable.notNull() && !mDrawable->isRoot() && getParent())
		{
			// Do not return cached position if you have a parent, recalculate
			// until all dirtying is done correctly.
			LLVector3 position_region =
				((LLViewerObject*)getParent())->getPositionRegion() +
				getPosition() * getParent()->getRotation();
			mPositionAgent = mRegionp->getPosAgentFromRegion(position_region);
		}
		else
		{
			mPositionAgent = mRegionp->getPosAgentFromRegion(getPosition());
		}
	}
	return mPositionAgent;
}

const LLVector3& LLViewerObject::getPositionRegion() const
{
	if (!isRoot())
	{
		LLViewerObject* parent = (LLViewerObject*)getParent();
		mPositionRegion = parent->getPositionRegion() +
						  getPosition() * parent->getRotation();
	}
	else
	{
		mPositionRegion = getPosition();
	}

	return mPositionRegion;
}

const LLVector3 LLViewerObject::getPositionEdit() const
{
	if (isRootEdit())
	{
		return getPosition();
	}
	else
	{
		LLViewerObject* parent = (LLViewerObject*)getParent();
		LLVector3 position_edit = parent->getPositionEdit() +
								  getPosition() * parent->getRotationEdit();
		return position_edit;
	}
}

const LLVector3 LLViewerObject::getRenderPosition() const
{
	if (mDrawable.notNull() && mDrawable->isState(LLDrawable::RIGGED))
	{
		if (isRoot())
		{
			LLVOAvatarPuppet* puppet = getPuppetAvatar();
			if (puppet)
			{
				F32 fixup;
				if (puppet->hasPelvisFixup(fixup))
				{
					LLVector3 pos = mDrawable->getPositionAgent();
					pos[VZ] += fixup;
					return pos;
				}
			}
		}
		LLVOAvatar* avatar = getAvatar();
#if 0
		if (avatar && !avatar->isPuppetAvatar())
		{
			return avatar->getRenderPosition();
		}
#else
		if (avatar && !getPuppetAvatar())
		{
			return avatar->getPositionAgent();
		}
#endif
	}

	if (mDrawable.isNull() || mDrawable->getGeneration() < 0)
	{
		return getPositionAgent();
	}

	return mDrawable->getPositionAgent();
}

const LLQuaternion LLViewerObject::getRenderRotation() const
{
	bool has_drawable = mDrawable.notNull();

	if (has_drawable && mDrawable->isState(LLDrawable::RIGGED) &&
		!isAnimatedObject())
	{
		return LLQuaternion();
	}

	if (!has_drawable || mDrawable->isStatic())
	{
		return getRotationEdit();
	}

	if (!mDrawable->isRoot())
	{
		return getRotation() *
			   LLQuaternion(mDrawable->getParent()->getWorldMatrix());
	}

	return LLQuaternion(mDrawable->getWorldMatrix());
}

const LLMatrix4& LLViewerObject::getRenderMatrix() const
{
	return mDrawable->getWorldMatrix();
}

const LLQuaternion LLViewerObject::getRotationRegion() const
{
	if (((LLXform*)this)->isRoot())
	{
		return getRotation();
	}
	return getRotation() * getParent()->getRotation();
}

const LLQuaternion LLViewerObject::getRotationEdit() const
{
	if (((LLXform*)this)->isRootEdit())
	{
		return getRotation();
	}
	return getRotation() * getParent()->getRotation();
}

void LLViewerObject::setPositionAbsoluteGlobal(const LLVector3d& pos_global)
{
	if (isAttachment())
	{
		LLVector3 new_pos = mRegionp->getPosRegionFromGlobal(pos_global);
		if (isRootEdit())
		{
			new_pos -= mDrawable->mXform.getParent()->getWorldPosition();
			LLQuaternion world_rotation =
				mDrawable->mXform.getParent()->getWorldRotation();
			new_pos = new_pos * ~world_rotation;
		}
		else
		{
			LLViewerObject* parentp = (LLViewerObject*)getParent();
			new_pos -= parentp->getPositionAgent();
			new_pos = new_pos * ~parentp->getRotationRegion();
		}
		setPositionLocal(new_pos);

		if (mParent && mParent->isAvatar())
		{
			// We have changed the position of an attachment, so we need to
			// clamp it
			((LLVOAvatar*)mParent)->clampAttachmentPositions();
		}
	}
	else if (isRoot())
	{
		setPositionRegion(mRegionp->getPosRegionFromGlobal(pos_global));
	}
	else
	{
		// The relative position with the parent is not constant
		LLViewerObject* parent = (LLViewerObject*)getParent();
		// RN: this assumes we are only calling this function from the edit
		// tools
		gPipeline.updateMoveNormalAsync(parent->mDrawable);

		LLVector3 pos_local = mRegionp->getPosRegionFromGlobal(pos_global) -
							  parent->getPositionRegion();
		pos_local = pos_local * ~parent->getRotationRegion();
		setPositionLocal(pos_local);
	}

	// RN: assumes we always want to snap the object when calling this function
	gPipeline.updateMoveNormalAsync(mDrawable);
}

void LLViewerObject::setPositionLocal(const LLVector3& pos, bool damped)
{
	if (getPosition() != pos)
	{
		setChanged(TRANSLATED | SILHOUETTE);
	}

	setPosition(pos);
	updateDrawable(damped);
	if (isRoot())
	{
		// Position caches need to be up to date on root objects
		updatePositionCaches();
	}
}

void LLViewerObject::setPositionGlobal(const LLVector3d& pos_global,
									   bool damped)
{
	if (isAttachment())
	{
		LLVector3 new_pos;
		if (isRootEdit())
		{
			new_pos = mRegionp->getPosRegionFromGlobal(pos_global);
			new_pos = new_pos - mDrawable->mXform.getParent()->getWorldPosition();

			LLQuaternion inv_world_rot =
				mDrawable->mXform.getParent()->getWorldRotation();
			inv_world_rot.transpose();

			new_pos = new_pos * inv_world_rot;
			setPositionLocal(new_pos);
		}
		else
		{
			// Assumes parent is root editable (root of attachment)
			new_pos = mRegionp->getPosRegionFromGlobal(pos_global);
			new_pos = new_pos - mDrawable->mXform.getParent()->getWorldPosition();
			LLVector3 delta_pos = new_pos - getPosition();

			LLQuaternion invRotation = mDrawable->getRotation();
			invRotation.transpose();

			delta_pos = delta_pos * invRotation;

			// *FIXME: is this right ?  Should not we be calling the
			// LLViewerObject version of setPosition ?
			LLVector3 old_pos = mDrawable->mXform.getParent()->getPosition();
			mDrawable->mXform.getParent()->setPosition(old_pos + delta_pos);
			setChanged(TRANSLATED | SILHOUETTE);
		}
		if (mParent && mParent->isAvatar())
		{
			// We have changed the position of an attachment, so we need to
			// clamp it
			((LLVOAvatar*)mParent)->clampAttachmentPositions();
		}
	}
	else if (isRoot())
	{
		setPositionRegion(mRegionp->getPosRegionFromGlobal(pos_global));
	}
	else
	{
		// The relative position with the parent is constant, but the parent's
		// position needs to be changed
		LLVector3d position_offset;
		position_offset.set(getPosition()*getParent()->getRotation());
		LLVector3d new_pos_global = pos_global - position_offset;
		((LLViewerObject*)getParent())->setPositionGlobal(new_pos_global);
	}
	updateDrawable(damped);
}

void LLViewerObject::setPositionParent(const LLVector3& pos_parent,
									   bool damped)
{
	// Set position relative to parent, if no parent, relative to region
	if (!isRoot())
	{
		setPositionLocal(pos_parent, damped);
#if 0
		updateDrawable(damped);
#endif
	}
	else
	{
		setPositionRegion(pos_parent);
	}
}

void LLViewerObject::setPositionRegion(const LLVector3& pos_region)
{
	if (isRootEdit())
	{
		setPositionLocal(pos_region);
		mPositionRegion = pos_region;
		if (mRegionp)
		{
			mPositionAgent = mRegionp->getPosAgentFromRegion(mPositionRegion);
		}
	}
	else
	{
		LLViewerObject* parent = (LLViewerObject*)getParent();
		setPositionLocal(pos_region - parent->getPositionRegion() *
						 ~parent->getRotationRegion());
	}
}

void LLViewerObject::setPositionAgent(const LLVector3& pos_agent)
{
	if (mRegionp)
	{
		setPositionRegion(mRegionp->getPosRegionFromAgent(pos_agent));
	}
}

// Identical to setPositionRegion() except it checks for child-joints and does
// not also move the joint-parent. *TODO: implement similar intelligence for
// joint-parents toward their joint-children
void LLViewerObject::setPositionEdit(const LLVector3& pos_edit, bool damped)
{
	if (isRootEdit())
	{
		if (!mRegionp)
		{
			llwarns << "Region not set; position unchanged for object Id: "
					<< mID << llendl;
			return;
		}
		setPositionLocal(pos_edit, damped);
		mPositionRegion = pos_edit;
		mPositionAgent = mRegionp->getPosAgentFromRegion(mPositionRegion);
	}
	else
	{
		// The relative position with the parent is constant, but the parent's
		// position needs to be changed
		LLVector3 position_offset = getPosition() * getParent()->getRotation();
		((LLViewerObject*)getParent())->setPositionEdit(pos_edit -
														position_offset);
		updateDrawable(damped);
	}
}

LLViewerObject* LLViewerObject::getRootEdit() const
{
	const LLViewerObject* root = this;
	while (root->mParent && !root->mParent->isAvatar())
	{
		root = (LLViewerObject*)root->mParent;
	}
	return (LLViewerObject*)root;
}

bool LLViewerObject::lineSegmentIntersect(const LLVector4a& start,
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
	return false;
}

bool LLViewerObject::lineSegmentBoundingBox(const LLVector4a& start,
											const LLVector4a& end)
{
	if (mDrawable.isNull() || mDrawable->isDead())
	{
		return false;
	}

	const LLVector4a* ext = mDrawable->getSpatialExtents();

	// VECTORIZE THIS
	LLVector4a center;
	center.setAdd(ext[1], ext[0]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(ext[1], ext[0]);
	size.mul(0.5f);

	return LLLineSegmentBoxIntersect(start, end, center, size);
}

void LLViewerObject::setMediaType(U8 media_type)
{
	// *TODO: what if we do not have a media pointer ?
	if (mMedia && mMedia->mMediaType != media_type)
	{
		mMedia->mMediaType = media_type;
		// *TODO: update materials with new image
	}
}

void LLViewerObject::setMediaURL(const std::string& media_url)
{
	if (!mMedia)
	{
		mMedia = new LLViewerObjectMedia;
		mMedia->mMediaURL = media_url;
		mMedia->mPassedWhitelist = false;
		// *TODO: update materials with new image
	}
	else if (mMedia->mMediaURL != media_url)
	{
		mMedia->mMediaURL = media_url;
		mMedia->mPassedWhitelist = false;
		// *TODO: update materials with new image
	}
}

bool LLViewerObject::setMaterial(U8 material)
{
	bool res = LLPrimitive::setMaterial(material);
	if (res)
	{
		setChanged(TEXTURE);
	}
	return res;
}

void LLViewerObject::setNumTEs(U8 num_tes)
{
	U8 old_num_tes = getNumTEs();
	if (num_tes == old_num_tes)
	{
		return;
	}

	if (num_tes)
	{
		// Duplicate last TE into new ones, when increasing the number of TEs:
		// this happens whenever you slice an object, or hollow it while
		// editing it, for example.
		if (old_num_tes && num_tes > old_num_tes)
		{
			mTEImages.reserve(num_tes);
			mTENormalMaps.reserve(num_tes);
			mTESpecularMaps.reserve(num_tes);
			U8 last = old_num_tes - 1;
			LLViewerTexture* diffusep = mTEImages[last].get();
			LLViewerTexture* normalp = mTENormalMaps[last].get();
			LLViewerTexture* specularp = mTESpecularMaps[last].get();
			for (U8 i = old_num_tes; i < num_tes; ++i)
			{
				mTEImages.emplace_back(diffusep);
				mTENormalMaps.emplace_back(normalp);
				mTESpecularMaps.emplace_back(specularp);
			}
		}
		else
		{
			mTEImages.resize(num_tes);
			mTENormalMaps.resize(num_tes);
			mTESpecularMaps.resize(num_tes);
		}
	}
	else if (!mTEImages.empty())
	{
		mTEImages.clear();
		mTENormalMaps.clear();
		mTESpecularMaps.clear();
	}

	LLPrimitive::setNumTEs(num_tes);
	setChanged(TEXTURE);

	// Duplicate any GLTF material in the same way.
	if (old_num_tes > 0 && old_num_tes < num_tes)
	{
		LLTextureEntry* srcp = getTE(old_num_tes - 1);
		if (srcp)
		{
			LLGLTFMaterial* matp = srcp->getGLTFMaterial();
			LLGLTFMaterial* omatp = srcp->getGLTFMaterialOverride();
			if (matp && omatp)
			{
				const LLUUID& mat_id = getRenderMaterialID(old_num_tes - 1);
				for (U8 i = old_num_tes; i < num_tes; ++i)
				{
					setRenderMaterialID(i, mat_id, false);

					LLTextureEntry* tep = getTE(i);
					if (!tep)
					{
						continue;
					}

					tep->setGLTFMaterialOverride(new LLGLTFMaterial(*omatp));
					LLGLTFMaterial* rmatp = new LLFetchedGLTFMaterial();
					*rmatp = *matp;
					rmatp->applyOverride(*omatp);
					tep->setGLTFRenderMaterial(rmatp);
				}
			}
		}
	}

	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
	}
}

void LLViewerObject::sendMaterialUpdate() const
{
	LLViewerRegion* regionp = getRegion();
	if (!regionp) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectMaterial);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID,	mLocalID);
	msg->addU8Fast(_PREHASH_Material, getMaterial());
	msg->sendReliable(regionp->getHost());
}

// formerly send_object_shape(LLViewerObject *object)
void LLViewerObject::sendShapeUpdate()
{
	LLViewerRegion* regionp = getRegion();
	if (!regionp) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectShape);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, mLocalID);

	LLVolumeMessage::packVolumeParams(&getVolume()->getParams(), msg);

	msg->sendReliable(regionp->getHost());
}

void LLViewerObject::sendTEUpdate() const
{
	LLViewerRegion* regionp = getRegion();
	if (!regionp) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectImage);

	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, mLocalID);
	if (mMedia)
	{
		msg->addString(_PREHASH_MediaURL, mMedia->mMediaURL);
	}
	else
	{
		msg->addString(_PREHASH_MediaURL, NULL);
	}

	// *TODO: send media type

	packTEMessage(msg);

	msg->sendReliable(regionp->getHost());
}

LLViewerTexture* LLViewerObject::getBakedTextureForMagicId(const LLUUID& id)
{
	if (!LLAvatarAppearanceDictionary::isBakedImageId(id))
	{
		return NULL;
	}

	LLViewerObject* root = getRootEdit();
	bool is_animesh = root && root->isAnimatedObject();

	LLVOAvatar* avatarp = is_animesh ? root->getAvatarAncestor() : getAvatar();
	if (avatarp)
	{
		EBakedTextureIndex tex_idx =
			LLAvatarAppearanceDictionary::assetIdToBakedTextureIndex(id);
		LLViewerTexture* baked_tex = avatarp->getBakedTexture(tex_idx);
		if (baked_tex && !baked_tex->isMissingAsset())
		{
			return baked_tex;
		}
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	return LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
													 LLGLTexture::BOOST_NONE,
													 LLViewerTexture::LOD_TEXTURE);
}

void LLViewerObject::updateAvatarMeshVisibility(const LLUUID& id,
												const LLUUID& old_id)
{
	LLVOAvatar* avatarp = getAvatar();
	if (id != old_id && avatarp &&
		(LLAvatarAppearanceDictionary::isBakedImageId(old_id) ||
		 LLAvatarAppearanceDictionary::isBakedImageId(id)))
	{
		avatarp->updateMeshVisibility();
	}
}

void LLViewerObject::setTE(U8 te, const LLTextureEntry& texture_entry)
{
	LLUUID old_image_id;
	LLTextureEntry* tep = getTE(te);
	if (tep)
	{
		old_image_id = tep->getID();
	}
	LLPrimitive::setTE(te, texture_entry);
	tep = getTE(te);
	if (!tep)
	{
		return;
	}

	const LLUUID& image_id = tep->getID();
	LLViewerTexture* baked_tex = getBakedTextureForMagicId(image_id);
	if (baked_tex)
	{
		mTEImages[te] = baked_tex;
	}
	else
	{
		mTEImages[te] =
			LLViewerTextureManager::getFetchedTexture(image_id,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_NONE,
													  LLViewerTexture::LOD_TEXTURE);
	}
	updateAvatarMeshVisibility(image_id, old_image_id);

	updateTEMaterialTextures(te);
}

LLViewerFetchedTexture* LLViewerObject::getFetchedTexForMat(const LLUUID& id,
															F32 vsize, U32 prio)
{
	if (id.isNull())
	{
		return NULL;
	}

	LLViewerFetchedTexture* texp = NULL;

	if (LLAvatarAppearanceDictionary::isBakedImageId(id))
	{
		LLViewerTexture* btexp = getBakedTextureForMagicId(id);
		if (btexp)
		{
			texp = btexp->asFetched();
		}
	}
	else
	{
		texp = LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
														 (LLGLTexture::EBoostLevel)prio,
														 LLViewerTexture::LOD_TEXTURE);
	}

	if (texp)
	{
		texp->addTextureStats(vsize);
	}

	return texp;
}

void LLViewerObject::updateTEMaterialTextures(U8 te)
{
	LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		return;
	}

	if (tep->getMaterialParams().notNull())
	{
		const LLUUID& norm_id = tep->getMaterialParams()->getNormalID();
		mTENormalMaps[te] =
			LLViewerTextureManager::getFetchedTexture(norm_id,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_ALM,
													  LLViewerTexture::LOD_TEXTURE);

		const LLUUID& spec_id = tep->getMaterialParams()->getSpecularID();
		mTESpecularMaps[te] =
			LLViewerTextureManager::getFetchedTexture(spec_id,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_ALM,
													  LLViewerTexture::LOD_TEXTURE);
	}

	const LLUUID& mat_id = getRenderMaterialID(te);
	LLFetchedGLTFMaterial* matp =
		(LLFetchedGLTFMaterial*)tep->getGLTFRenderMaterial();
	if (matp)
	{
		if (mat_id.isNull())
		{
			tep->setGLTFMaterial(NULL);
			return;
		}
	}
	else if (mat_id.notNull())
	{
		matp = (LLFetchedGLTFMaterial*)gGLTFMaterialList.getMaterial(mat_id);
		if (!matp) return;	// Maybe paranoia ?... Better safe than sorry. HB

		if (matp->isFetching())
		{
			// Material is not loaded yet, rebuild draw info when the object
			// finishes loading.
			matp->onMaterialComplete([id = mID]
									 {
										LLViewerObject* objp =
											gObjectList.findObject(id);
										if (!objp) return;
										LLViewerRegion* regionp =
											objp->getRegion();
										if (regionp)
										{
											regionp->loadCacheMiscExtras(objp);
										}
										objp->markForUpdate(false);
									 });
		}
		tep->setGLTFMaterial(matp);
	}

	if (!matp)
	{
		return;	// Nothing else to do.
	}

	// When enough memory, fetch at a good resolution, else use the discard
	// bias to reduce the initial resolution of the textures. HB
	constexpr F32 MAX_VSIZE = 512.f * 512.f;
	constexpr F32 BIAS_SCALER = 3.f / 5.f;	// Bias 0 to 5 -> 0 to 3
	// Factor will be between 1 and 1/4, depending on discard bias
	F32 factor = 1.f / (1.f +
						BIAS_SCALER * LLViewerTexture::sDesiredDiscardBias);
	F32 vsize = MAX_VSIZE * factor * factor;

	matp->mBaseColorTexture =
		getFetchedTexForMat(matp->mTextureId[BASECOLIDX], vsize);
	matp->mNormalTexture = getFetchedTexForMat(matp->mTextureId[NORMALIDX],
											  vsize, LLGLTexture::BOOST_ALM);
	matp->mMetallicRoughnessTexture =
		getFetchedTexForMat(matp->mTextureId[MROUGHIDX], vsize);
	matp->mEmissiveTexture =
		getFetchedTexForMat(matp->mTextureId[EMISSIVEIDX], vsize,
							LLGLTexture::BOOST_ALM);
}

void LLViewerObject::refreshBakeTexture()
{
	LL_DEBUGS("AttachmentBakes") << "Refreshing attachment bake textures for object "
								 << mID << LL_ENDL;
	bool changed = false;
	for (U8 te = 0, count = getNumTEs(); te < count; ++te)
	{
		LLTextureEntry* tep = getTE(te);
		if (!tep) continue;

		const LLUUID& image_id = tep->getID();
		if (LLAvatarAppearanceDictionary::isBakedImageId(image_id))
		{
			LLViewerTexture* baked_tex = getBakedTextureForMagicId(image_id);
			if (baked_tex)
			{
				LL_DEBUGS("AttachmentBakes") << "Face index: " << (S32)te
											 << " - Bake Id: " << image_id
											 << " - Baked texture Id: "
											 << baked_tex->getID() << LL_ENDL;
				changeTEImage(te, baked_tex);
				changed = true;
			}
		}
	}
	// *HACK: strangely, we need force a LLVOVolume rebuild to see the texture
	// actually changing on the prim. It is probably a result of how textures
	// are batched in the Cool VL Viewer.
	if (changed && mDrawable.notNull())
	{
		LLVOVolume* volp = mDrawable->getVOVolume();
		if (volp)
		{
			volp->tempSetLOD(0);
			volp->faceMappingChanged();
		}
	}
}

bool LLViewerObject::hasRenderMaterialParams() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_RENDER_MATERIAL);
}

void LLViewerObject::setHasRenderMaterialParams(bool has_materials)
{
	if (hasRenderMaterialParams() != has_materials)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_RENDER_MATERIAL,
							   has_materials, true);
	}
}

const LLUUID& LLViewerObject::getRenderMaterialID(U8 te) const
{
	LLRenderMaterialParams* paramsp = getMaterialRenderParams();
	return paramsp ? paramsp->getMaterial(te) : LLUUID::null;
}

static void set_te_overide_mat(const LLUUID& obj_id, U8 te)
{
	LLViewerObject* objp = gObjectList.findObject(obj_id);
	if (!objp)
	{
		return;
	}
	LLTextureEntry* tep = objp->getTE(te);
	if (!tep)
	{
		return;
	}
	const LLGLTFMaterial* matp = tep->getGLTFMaterial();
	if (!matp)
	{
		return;
	}
	const LLGLTFMaterial* omatp = tep->getGLTFMaterialOverride();
	if (omatp)
	{
		LLGLTFMaterial* rmatp = new LLFetchedGLTFMaterial();
		*rmatp = *matp;
		rmatp->applyOverride(*omatp);
		tep->setGLTFRenderMaterial(rmatp);
	}
}

// Implementation is delicate: if update is bound for server, it should always
// null out GLTFRenderMaterial and clear GLTFMaterialOverride even if Ids have
// not changed (the case where Ids have not changed indicates the user has
// reapplied the original material, in which case overrides should be dropped),
// otherwise it should only null out the render material where Ids or overrides
// have changed (the case where Ids have changed but overrides are still
// present is from unsynchronized updates from the simulator, or synchronized
// updates with solely transform overrides).
void LLViewerObject::setRenderMaterialID(S32 te_in, const LLUUID& id,
										 bool update_server, bool local_origin)
{
	S32 end_idx = getNumTEs();
	if (te_in >= end_idx)
	{
		llwarns << "Out of bound te: " << te_in << ". Aborted." << llendl;
		return;
	}
	S32 start_idx;
	if (te_in < 0)
	{
		start_idx = 0;
	}
	else
	{
		start_idx = te_in;
		end_idx = llmin(start_idx + 1, end_idx);
	}

	// If needed, enable the "GLTF" debug tag for this method call with this
	// debugged LLViewerObject instance. HB
	bool set_debug_tag = mDebugUpdateMsg &&
						 LLError::getTagLevel("GLTF") != LLError::LEVEL_DEBUG;
	if (set_debug_tag)
	{
		HBFloaterDebugTags::setTag("GLTF", true);
	}

	LL_DEBUGS("GLTF") << "Called for object " << mID
					  << " to set PBR material " << id << " on faces "
					  << start_idx << " to " << end_idx << ". update_server = "
					  << (update_server ? "true" : "false") << LL_ENDL;

	LLRenderMaterialParams* paramsp = NULL;
	LLFetchedGLTFMaterial* matp = NULL;
	if (id.isNull())
	{
		// Get the parameter block if it exists.
		paramsp = getMaterialRenderParams();
		LL_DEBUGS("GLTF") << "Parameter block "
						  << (paramsp ? "exists." : "does not exist.")
						  << LL_ENDL;
	}
	else
	{
		// Get the existing block or create one when missing, so to set the new
		// material.
		paramsp =
			(LLRenderMaterialParams*)getExtraParameterEntryCreate(LLNetworkData::PARAMS_RENDER_MATERIAL);
		if (!paramsp)
		{
			llwarns << "Could not create an extra parameter entry for: "
					<< te_in << ". Aborted." << llendl;
			return;
		}
		matp = gGLTFMaterialList.getMaterial(id);
		LL_DEBUGS("GLTF") << "PBR material " << (matp ? "found" : "not found")
						  << " in the list." << LL_ENDL;
	}

	// Update local state
	for (U8 te = start_idx; te < end_idx; ++te)
	{
		LLTextureEntry* tep = getTE(te);
		if (!tep) continue;	// Paranoia. HB

		// If local_origin=false (i.e. it is from the server), we know the
		// material has updated or been created, because extra params are
		// checked for equality on unpacking. In that case, checking the
		// material Id for inequality would not work, because the material Id
		// has already been set.
		bool material_changed = !local_origin ||
								(paramsp && paramsp->getMaterial(te) != id);
		if (update_server)
		{
			// Clear most overrides so the render material better matches the
			// material Id (preserve transforms). If overrides become
			// passthrough, set the overrides to NULL.
			if (tep->setBaseMaterial())
			{
				material_changed = true;
				LL_DEBUGS("GLTF") << "Material reset to base material on face: "
								  << U32(te) << LL_ENDL;
			}
		}
		if (update_server || material_changed)
		{
			tep->setGLTFRenderMaterial(NULL);
			LL_DEBUGS("GLTF") << "Render material NULLed out on face: "
							  << U32(te) << LL_ENDL;
		}
		if (matp != tep->getGLTFMaterial())
		{
			tep->setGLTFMaterial(matp, !update_server);
			LL_DEBUGS("GLTF") << "New material set on face: " << U32(te)
							  << LL_ENDL;
		}
		if (material_changed && matp && tep->getGLTFMaterialOverride())
		{
			// Sometimes, the material may change out from underneath the
			// overrides. This is usually due to the server sending a new
			// material Id, but the overrides have not changed due to being
			// only texture transforms. Re-apply the overrides to the render
			// material here, if present.
			matp->onMaterialComplete([obj_id = mID, te]()
									 {
										set_te_overide_mat(obj_id, te);
									 });
		}
	}

	// Signal to render pipeline that render batches must be rebuilt for this
	// object
	if (gUsePBRShaders)
	{
		if (matp)
		{
			matp->onMaterialComplete([obj_id = getID()]()
									 {
										LLViewerObject* objp =
											gObjectList.findObject(obj_id);
										if (objp)
										{
											objp->rebuildMaterial();
										}
									 });
		}
		else
		{
			rebuildMaterial();
		}
	}

	// Predictively update LLRenderMaterialParams (do not wait for server)
	if (paramsp)
	{
		// Update existing parameter block
		for (U8 te = start_idx; te < end_idx; ++te)
		{
			paramsp->setMaterial(te, id);
		}
	}

	if (update_server)
	{
		// Update via ModifyMaterialParams cap (server will echo back changes)
		for (U8 te = start_idx; te < end_idx; ++te)
		{
			// This sends a cleared version of this object's current material
			// override, but the override should already be cleared due to
			// calling setBaseMaterial() above.
			LLGLTFMaterialList::queueApply(this, te, id);
		}
	}
	else
	{
		// Land impact may have changed
		mCostStale = true;
		LLViewerObject* rootp = (LLViewerObject*)getRootEdit();
		if (rootp)	// Paranoia
		{
			// Note: this is harmlessly redundant for Blinn-Phong material
			// updates, as the root prim currently gets set stale anyway due
			// to other property updates. But it is needed for GLTF material
			// Id updates. Cosmic,2023-06-27
			rootp->mCostStale = true;
		}
	}

	if (set_debug_tag)
	{
		HBFloaterDebugTags::setTag("GLTF", false);
	}
}

void LLViewerObject::setRenderMaterialIDs(const LLRenderMaterialParams* paramsp,
										  bool local_origin)
{
	if (local_origin)
	{
		return;	// Nothing to do
	}
	if (paramsp)
	{
		for (U8 te = 0, count = getNumTEs(); te < count; ++te)
		{
			// We know material_params has updated or been created, because
			// extra params are checked for equality on unpacking.
			setRenderMaterialID(te, paramsp->getMaterial(te), false, false);
		}
	}
	else
	{
		for (U8 te = 0, count = getNumTEs(); te < count; ++te)
		{
			setRenderMaterialID(te, LLUUID::null, false, false);
		}
	}
}

void LLViewerObject::rebuildMaterial()
{
	faceMappingChanged();
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
	}
}

void LLViewerObject::shrinkWrap()
{
	if (!mShouldShrinkWrap)
	{
		mShouldShrinkWrap = true;
		if (mDrawable)
		{
			// We were not shrink-wrapped before but we are now, so update the
			// spatial partition.
			gPipeline.markPartitionMove(mDrawable);
		}
	}
}

void LLViewerObject::setTEImage(U8 te, LLViewerTexture* texp)
{
	if (te != 255 && te < getNumTEs() && texp && mTEImages[te] != texp)
	{
		LLUUID old_image_id;
		LLTextureEntry* tep = getTE(te);
		if (tep)
		{
			old_image_id = tep->getID();
		}

		const LLUUID& image_id = texp->getID();
		LLPrimitive::setTETexture(te, image_id);

		LLViewerTexture* baked_texp = getBakedTextureForMagicId(image_id);
		mTEImages[te] = baked_texp ? baked_texp : texp;
		updateAvatarMeshVisibility(image_id, old_image_id);
		setChanged(TEXTURE);

		if (mDrawable.notNull())
		{
			gPipeline.markTextured(mDrawable);
		}
	}
}

S32 LLViewerObject::setTETextureCore(U8 te, LLViewerTexture* texp)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep || !texp) return 0;

	const LLUUID& tex_id = texp->getID();
	LLUUID old_tex_id = tep->getID();
	if (tex_id.notNull() && old_tex_id == tex_id)
	{
		return 0;
	}

	S32 retval = LLPrimitive::setTETexture(te, tex_id);

	LLViewerTexture* baked_texp = getBakedTextureForMagicId(tex_id);
	mTEImages[te] = baked_texp ? baked_texp : texp;
	updateAvatarMeshVisibility(tex_id, old_tex_id);
	setChanged(TEXTURE);

	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
	}

	return retval;
}

S32 LLViewerObject::setTENormalMapCore(U8 te, LLViewerTexture* texp)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep) return 0;

	const LLUUID& tex_id = texp ? texp->getID() : LLUUID::null;
	if (tep->getID() != tex_id || tex_id.isNull())
	{
		LLMaterial* matp = tep->getMaterialParams();
		if (matp)
		{
			LL_DEBUGS("Materials") << "te = "<< (S32)te
								   << ", setting normal map id = " << tex_id
								   << LL_ENDL;
			matp->setNormalID(tex_id);
		}
	}

	changeTENormalMap(te, texp);

	return TEM_CHANGE_TEXTURE;
}

S32 LLViewerObject::setTESpecularMapCore(U8 te, LLViewerTexture* texp)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep) return 0;

	const LLUUID& tex_id = texp ? texp->getID() : LLUUID::null;
	if (tep->getID() != tex_id || tex_id.isNull())
	{
		LLMaterial* matp = tep->getMaterialParams();
		if (matp)
		{
			LL_DEBUGS("Materials") << "te = "<< (S32)te
								   << ", setting specular map id = " << tex_id
								   << LL_ENDL;
			matp->setSpecularID(tex_id);
		}
	}

	changeTESpecularMap(te, texp);

	return TEM_CHANGE_TEXTURE;
}

//virtual
void LLViewerObject::changeTEImage(S32 index, LLViewerTexture* texp)
{
	if (index >= 0 && index < getNumTEs())
	{
		mTEImages[index] = texp;
	}
}

//virtual
void LLViewerObject::changeTENormalMap(S32 index, LLViewerTexture* texp)
{
	if (index >= 0 && index < getNumTEs())
	{
		mTENormalMaps[index] = texp;
		refreshMaterials();
	}
}

//virtual
void LLViewerObject::changeTESpecularMap(S32 index, LLViewerTexture* texp)
{
	if (index >= 0 && index < getNumTEs())
	{
		mTESpecularMaps[index] = texp;
		refreshMaterials();
	}
}

//virtual
S32 LLViewerObject::setTETexture(U8 te, const LLUUID& tex_id)
{
	// Invalid host == get from the agent's sim
	LLViewerFetchedTexture* texp =
		LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT, true,
												  LLGLTexture::BOOST_NONE,
												  LLViewerTexture::LOD_TEXTURE);
	return setTETextureCore(te, texp);
}

//virtual
S32 LLViewerObject::setTENormalMap(U8 te, const LLUUID& tex_id)
{
	LLViewerFetchedTexture* texp = NULL;
	if (tex_id.notNull())
	{
		texp =
			LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT,
													  true,
													  LLGLTexture::BOOST_ALM,
													  LLViewerTexture::LOD_TEXTURE);
	}
	return setTENormalMapCore(te, texp);
}

//virtual
S32 LLViewerObject::setTESpecularMap(U8 te, const LLUUID& tex_id)
{
	LLViewerFetchedTexture* texp = NULL;
	if (tex_id.notNull())
	{
		texp =
			LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT,
													  true,
													  LLGLTexture::BOOST_ALM,
													  LLViewerTexture::LOD_TEXTURE);
	}
	return setTESpecularMapCore(te, texp);
}

//virtual
S32 LLViewerObject::setTEColor(U8 te, const LLColor3& color)
{
	return setTEColor(te, LLColor4(color));
}

//virtual
S32 LLViewerObject::setTEColor(U8 te, const LLColor4& color)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (color != tep->getColor())
	{
		retval = LLPrimitive::setTEColor(te, color);
		if (mDrawable.notNull() && retval)
		{
			// These should only happen on updates which are not the initial
			// update.
			dirtyMesh();
		}
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEBumpmap(U8 te, U8 bump)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (bump != tep->getBumpmap())
	{
		retval = LLPrimitive::setTEBumpmap(te, bump);
		setChanged(TEXTURE);
		if (mDrawable.notNull() && retval)
		{
			gPipeline.markTextured(mDrawable);
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY);
		}
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTETexGen(U8 te, U8 texgen)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (texgen != tep->getTexGen())
	{
		retval = LLPrimitive::setTETexGen(te, texgen);
		setChanged(TEXTURE);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEMediaTexGen(U8 te, U8 media)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (media != tep->getMediaTexGen())
	{
		retval = LLPrimitive::setTEMediaTexGen(te, media);
		setChanged(TEXTURE);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEShiny(U8 te, U8 shiny)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (shiny != tep->getShiny())
	{
		retval = LLPrimitive::setTEShiny(te, shiny);
		setChanged(TEXTURE);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEFullbright(U8 te, U8 fullbright)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (fullbright != tep->getFullbright())
	{
		retval = LLPrimitive::setTEFullbright(te, fullbright);
		setChanged(TEXTURE);
		if (mDrawable.notNull() && retval)
		{
			gPipeline.markTextured(mDrawable);
		}
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEMediaFlags(U8 te, U8 media_flags)
{
	// this might need work for media type
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (media_flags != tep->getMediaFlags())
	{
		retval = LLPrimitive::setTEMediaFlags(te, media_flags);
		setChanged(TEXTURE);
		if (mDrawable.notNull() && retval)
		{
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
			gPipeline.markTextured(mDrawable);
			// JC - probably only need this if changes texture coords
			//gPipeline.markRebuild(mDrawable);
		}
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEGlow(U8 te, F32 glow)
{
	S32 retval = 0;

	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
	}
	else if (glow != tep->getGlow())
	{
		retval = LLPrimitive::setTEGlow(te, glow);
		setChanged(TEXTURE);
		if (mDrawable.notNull() && retval)
		{
			gPipeline.markTextured(mDrawable);
		}
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEMaterialID(U8 te, const LLMaterialID& matidp)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< ", material " << matidp << llendl;
		return 0;
	}

	S32 retval = LLPrimitive::setTEMaterialID(te, matidp);
	refreshMaterials();
	LL_DEBUGS("Materials") << "Changed texture entry for te " << (S32)te
						   << " - object: " << mID << " - material: " << matidp
						   << " - retval = " << retval << LL_ENDL;
	return retval;
}

//virtual
S32 LLViewerObject::setTEMaterialParams(U8 te, const LLMaterialPtr paramsp)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
		return 0;
	}

	S32 retval = LLPrimitive::setTEMaterialParams(te, paramsp);
	setTENormalMap(te, paramsp ? paramsp->getNormalID() : LLUUID::null);
	setTESpecularMap(te, paramsp ? paramsp->getSpecularID() : LLUUID::null);
	refreshMaterials();
	LL_DEBUGS("Materials") << "Changed material params for te: " << (S32)te
						   << " - object: " << mID << " - retval = " << retval
						   << LL_ENDL;
	return retval;
}

//virtual
S32 LLViewerObject::setTEGLTFMaterialOverride(U8 te, LLGLTFMaterial* matp)
{
	LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
		return TEM_CHANGE_NONE;
	}

	LLFetchedGLTFMaterial* srcp =
		(LLFetchedGLTFMaterial*)tep->getGLTFMaterial();
	// If an override mat exists, we must also have a source mat; it could
	// however still be in flight (scrp == NULL) or being fetched.
	if (!srcp || srcp->isFetching())
	{
		if (srcp)
		{
			LL_DEBUGS("GLTF") << "GLTF material still being fetched for object "
							  << mID << LL_ENDL;
		}
		return TEM_CHANGE_NONE;
	}

	S32 retval = tep->setGLTFMaterialOverride(matp);
	if (retval)
	{
		if (matp)
		{
			LLFetchedGLTFMaterial* rmatp = new LLFetchedGLTFMaterial(*srcp);
			rmatp->applyOverride(*matp);
			tep->setGLTFRenderMaterial(rmatp);

			if (matp->hasLocalTextures())
			{
				for (LLGLTFMaterial::local_tex_map_t::const_iterator
						it = matp->mTrackingIdToLocalTexture.begin(),
						end =  matp->mTrackingIdToLocalTexture.end();
					 it != end; ++it)
				{
					LLLocalBitmap::associateGLTFMaterial(it->first, matp);
				}
			}

			return TEM_CHANGE_TEXTURE;
		}
		if (tep->setGLTFRenderMaterial(NULL))
		{
			return TEM_CHANGE_TEXTURE;
		}
	}
	return retval;
}

void LLViewerObject::refreshMaterials()
{
	setChanged(TEXTURE);
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
	}
}

//virtual
S32 LLViewerObject::setTEScale(U8 te, F32 s, F32 t)
{
	S32 retval = LLPrimitive::setTEScale(te, s, t);
	setChanged(TEXTURE);
	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}
	return retval;
}

//virtual
S32 LLViewerObject::setTEScaleS(U8 te, F32 s)
{
	S32 retval = LLPrimitive::setTEScaleS(te, s);

	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEScaleT(U8 te, F32 t)
{
	S32 retval = LLPrimitive::setTEScaleT(te, t);

	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEOffset(U8 te, F32 s, F32 t)
{
	S32 retval = LLPrimitive::setTEOffset(te, s, t);

	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEOffsetS(U8 te, F32 s)
{
	S32 retval = LLPrimitive::setTEOffsetS(te, s);

	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTEOffsetT(U8 te, F32 t)
{
	S32 retval = LLPrimitive::setTEOffsetT(te, t);

	if (mDrawable.notNull() && retval)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
	}

	return retval;
}

//virtual
S32 LLViewerObject::setTERotation(U8 te, F32 r)
{
	S32 retval = LLPrimitive::setTERotation(te, r);
	if (retval && mDrawable.notNull())
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
		shrinkWrap();
	}
	return retval;
}

LLViewerTexture* LLViewerObject::getTEImage(U8 face) const
{
	if (face < getNumTEs())
	{
		LLViewerTexture* texp = mTEImages[face];
		if (texp)
		{
			return texp;
		}
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	llwarns << "Requested image from invalid face: " << face << "/"
			<< getNumTEs() << llendl;

	return NULL;
}

LLViewerTexture* LLViewerObject::getTENormalMap(U8 face) const
{
	if (face < getNumTEs())
	{
		LLViewerTexture* texp = mTENormalMaps[face].get();
		if (texp)
		{
			return texp;
		}
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	llwarns << "Requested image from invalid face: " << face << " / "
			<< getNumTEs() << llendl;

	return NULL;
}

LLViewerTexture* LLViewerObject::getTESpecularMap(U8 face) const
{
	if (face < getNumTEs())
	{
		LLViewerTexture* texp = mTESpecularMaps[face];
		if (texp)
		{
			return texp;
		}
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	llwarns << "Requested image from invalid face: " << face << " / "
			<< getNumTEs() << llendl;

	return NULL;
}

bool LLViewerObject::isImageAlphaBlended(U8 te) const
{
	LLViewerTexture* texp = getTEImage(te);
	if (!texp)
	{
		return false;
	}

	GLenum format = texp->getPrimaryFormat();
	switch (format)
	{
		case GL_RGB:
			break;

		case GL_RGBA:
		case GL_ALPHA:
			return true;

		default:
			llwarns << "Unexpected tex format, returning no alpha." << llendl;
	}

	return false;
}

void LLViewerObject::fitFaceTexture(U8 face)
{
	llwarns << "Not implemented !" << llendl;
	llassert(false);
}

LLBBox LLViewerObject::getBoundingBoxAgent() const
{
	LLViewerObject* root_edit = (LLViewerObject*)getRootEdit();
	if (root_edit)
	{
		LLXform* root_parent = root_edit->getParent();
		if (!root_parent || !root_parent->isAvatar())
		{
			root_edit = NULL;
		}
	}
	LLVector3 position_agent;
	LLQuaternion rot;
	if (root_edit && root_edit->mDrawable.notNull() &&
		root_edit->mDrawable->getXform() &&
		root_edit->mDrawable->getXform()->getParent())
	{
		LLXform* parent_xform = root_edit->mDrawable->getXform()->getParent();
		position_agent = (getPositionEdit() *
						  parent_xform->getWorldRotation()) +
						 parent_xform->getWorldPosition();
		rot = getRotationEdit() * parent_xform->getWorldRotation();
	}
	else
	{
		position_agent = getPositionAgent();
		rot = getRotationRegion();
	}

	return LLBBox(position_agent, rot, getScale() * -0.5f, getScale() * 0.5f);
}

U32 LLViewerObject::getNumVertices() const
{
	U32 num_vertices = 0;

	if (mDrawable.notNull())
	{
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (facep)
			{
				num_vertices += facep->getGeomCount();
			}
		}
	}

	return num_vertices;
}

U32 LLViewerObject::getNumIndices() const
{
	U32 num_indices = 0;

	if (mDrawable.notNull())
	{
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (facep)
			{
				num_indices += facep->getIndicesCount();
			}
		}
	}

	return num_indices;
}

// Find the number of instances of this object's inventory that are of the
// given type
S32 LLViewerObject::countInventoryContents(LLAssetType::EType type)
{
	S32 count = 0;

	if (mInventory)
	{
		for (LLInventoryObject::object_list_t::const_iterator
				it = mInventory->begin(), end = mInventory->end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = *it;
			if (obj && obj->getType() == type)
			{
				++count;
			}
		}
	}

	return count;
}

void LLViewerObject::setDebugText(const std::string& utf8text)
{
	bool no_debug_text = utf8text.empty();
	if (no_debug_text && mHudTextString.empty())
	{
		if (mText)
		{
			mText->markDead();
			mText = NULL;
		}
		return;
	}

	if (!mText)
	{
		mText = (LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
		mText->setFont(LLFontGL::getFontSansSerif());
		mText->setVertAlignment(LLHUDText::ALIGN_VERT_TOP);
		mText->setMaxLines(-1);
		mText->setSourceObject(this);
		mText->setOnHUDAttachment(isHUDAttachment());
	}
	mText->setColor(no_debug_text ? mHudTextColor : LLColor4::white);
	mText->setStringUTF8(no_debug_text ? mHudTextString :  utf8text);
	mText->setZCompare(no_debug_text);
	mText->setDoFade(no_debug_text);
	updateText();
}

LLHUDIcon* LLViewerObject::setIcon(LLViewerTexture* texp, F32 scale)
{
	if (!mIcon)
	{
		mIcon = (LLHUDIcon*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_ICON);
		mIcon->setSourceObject(this);
		mIcon->setImage(texp);
		mIcon->setScale(scale);
	}
	else
	{
		mIcon->restartLifeTimer();
	}
	return mIcon;
}

LLViewerObject* LLViewerObject::getSubParent()
{
	return (LLViewerObject*)getParent();
}

const LLViewerObject* LLViewerObject::getSubParent() const
{
	return (const LLViewerObject*)getParent();
}

void LLViewerObject::updateText()
{
	if (!isDead() && mText.notNull())
	{
		LLVOAvatar* avatar = getAvatar();
		if (avatar)
		{
			mText->setHidden(avatar->isVisuallyMuted());
		}

		LLVector3 up_offset(0, 0, 0);
		up_offset.mV[2] = getScale().mV[VZ] * 0.6f;

		if (mDrawable.notNull())
		{
			mText->setPositionAgent(getRenderPosition() + up_offset);
		}
		else
		{
			mText->setPositionAgent(getPositionAgent() + up_offset);
		}
	}
}

bool LLViewerObject::isParticleSource() const
{
	return mPartSourcep.notNull() && !mPartSourcep->isDead();
}

void LLViewerObject::setParticleSource(const LLPartSysData& particle_params,
									   const LLUUID& owner_id)
{
	if (mPartSourcep)
	{
		deleteParticleSource();
	}

	LLPointer<LLViewerPartSourceScript> pss =
		LLViewerPartSourceScript::createPSS(this, particle_params);
	mPartSourcep = pss;

	if (mPartSourcep)
	{
		mPartSourcep->setOwnerUUID(owner_id);

		if (mPartSourcep->getImage()->getID() !=
				mPartSourcep->mPartSysData.mPartImageID)
		{
			const LLUUID& image_id = mPartSourcep->mPartSysData.mPartImageID;
			LLViewerTexture* image = gImgPixieSmall;
			if (image_id.notNull())
			{
				image = LLViewerTextureManager::getFetchedTexture(image_id);
			}
			mPartSourcep->setImage(image);
		}
	}

	gViewerPartSim.addPartSource(pss);
}

void LLViewerObject::unpackParticleSource(S32 block_num,
										  const LLUUID& owner_id)
{
	if (mPartSourcep.notNull() && mPartSourcep->isDead())
	{
		mPartSourcep = NULL;
	}

	if (mPartSourcep)
	{
		// If we have got one already, just update the existing source
		if (!LLViewerPartSourceScript::unpackPSS(this, mPartSourcep,
												 block_num))
		{
			mPartSourcep->setDead();
			mPartSourcep = NULL;
			return;
		}
	}
	else
	{
		LLPointer<LLViewerPartSourceScript> pss =
			LLViewerPartSourceScript::unpackPSS(this, NULL, block_num);
		if (!pss || LLMuteList::isMuted(owner_id, LLMute::flagParticles))
		{
			// Do not create the system
			return;
		}

		// We need to be able to deal with a particle source that has not
		// changed, but still got an update !
		LL_DEBUGS("Particles") << "Making particle system with owner "
							   << owner_id << " for object " << mID << LL_ENDL;
		mPartSourcep = pss;
		gViewerPartSim.addPartSource(pss);
	}

	if (mPartSourcep)
	{
		mPartSourcep->setOwnerUUID(owner_id);
		const LLUUID& image_id = mPartSourcep->mPartSysData.mPartImageID;
		if (mPartSourcep->getImage()->getID() != image_id)
		{
			LLViewerTexture* image = gImgPixieSmall;
			if (image_id.notNull())
			{
				image = LLViewerTextureManager::getFetchedTexture(image_id);
			}
			mPartSourcep->setImage(image);
		}
	}
}

void LLViewerObject::unpackParticleSource(LLDataPacker& dp,
										  const LLUUID& owner_id, bool legacy)
{
	if (mPartSourcep.notNull() && mPartSourcep->isDead())
	{
		mPartSourcep = NULL;
	}

	if (mPartSourcep)
	{
		// If we have got one already, just update the existing source
		if (!LLViewerPartSourceScript::unpackPSS(this, mPartSourcep, dp,
												 legacy))
		{
			mPartSourcep->setDead();
			mPartSourcep = NULL;
			return;
		}
	}
	else
	{
		LLPointer<LLViewerPartSourceScript> pss =
			LLViewerPartSourceScript::unpackPSS(this, NULL, dp, legacy);
		if (!pss || LLMuteList::isMuted(owner_id, LLMute::flagParticles))
		{
			// Do not create the system
			return;
		}

		// We need to be able to deal with a particle source that has not
		// changed, but still got an update !
		LL_DEBUGS("Particles") << "Making particle system with owner "
							   << owner_id << " for object " << mID << LL_ENDL;
		pss->setOwnerUUID(owner_id);
		mPartSourcep = pss;
		gViewerPartSim.addPartSource(pss);
	}

	if (mPartSourcep && mPartSourcep->getImage())
	{
		const LLUUID& image_id = mPartSourcep->mPartSysData.mPartImageID;
		if (mPartSourcep->getImage()->getID() != image_id)
		{
			LLViewerTexture* image = gImgPixieSmall;
			if (image_id.notNull())
			{
				image = LLViewerTextureManager::getFetchedTexture(image_id);
			}
			mPartSourcep->setImage(image);
		}
	}
}

void LLViewerObject::deleteParticleSource()
{
	if (mPartSourcep.notNull())
	{
		mPartSourcep->setDead();
		mPartSourcep = NULL;
	}
}

// virtual
void LLViewerObject::updateDrawable(bool force_damped)
{
	if (LL_UNLIKELY(isChanged(MOVED)) && mDrawable.notNull() &&
		!mDrawable->isState(LLDrawable::ON_MOVE_LIST))
	{
		bool damped_motion =
			 // Not shifted between regions this frame and...
			 !isChanged(SHIFTED) &&
			 // ...forced into damped motion by application logic or...
			 (force_damped ||
			   // ...not selected and...
			  (!mUserSelected &&
			    // ... is root or ...
			   (mDrawable->isRoot() ||
				// ... parent is not selected and ...
	 			(getParent() &&
				 !((LLViewerObject*)getParent())->mUserSelected)) &&
			   // ...is a volume object and...
			   getPCode() == LL_PCODE_VOLUME &&
			   // ...is not moving physically and...
			   getVelocity().isExactlyZero() &&
			   // ...was not created this frame.
			   mDrawable->getGeneration() != -1));
		gPipeline.markMoved(mDrawable, damped_motion);
	}
	clearChanged(SHIFTED);
}

// virtual, overridden by LLVOVolume
F32 LLViewerObject::getVObjRadius() const
{
	return mDrawable.notNull() ? mDrawable->getRadius() : 0.f;
}

void LLViewerObject::setAttachedSound(const LLUUID& audio_uuid,
									  const LLUUID& owner_id, F32 gain,
									  U8 flags)
{
	if (!gAudiop)
	{
		return;
	}

	if (audio_uuid.isNull())
	{
		if (!mAudioSourcep)
		{
			return;
		}
		if (mAudioSourcep->isLoop() && !mAudioSourcep->hasPendingPreloads())
		{
			// We do not clear the sound if it is a loop, it will go away on
			// its own. At least, this appears to be how the scripts work. The
			// attached sound ID is set to NULL to avoid it playing back when
			// the object rezzes in on non-looping sounds.
			LL_DEBUGS("AttachedSound") << "Clearing attached sound "
									   << mAudioSourcep->getCurrentData()->getID()
									   << LL_ENDL;
			gAudiop->cleanupAudioSource(mAudioSourcep);
			mAudioSourcep = NULL;
		}
		else if (flags & LL_SOUND_FLAG_STOP)
		{
			// Just shut off the sound
			mAudioSourcep->stop();
		}
		return;
	}
	if ((flags & LL_SOUND_FLAG_LOOP) &&
		mAudioSourcep && mAudioSourcep->isLoop() &&
		mAudioSourcep->getCurrentData() &&
		mAudioSourcep->getCurrentData()->getID() == audio_uuid)
	{
		LL_DEBUGS("AttachedSound") << "Already playing sound " << audio_uuid
								   << " on a loop, ignoring." << LL_ENDL;
		return;
	}

	// Do not clean up before previous sound is done. Solves: SL-33486
	if (mAudioSourcep && mAudioSourcep->isDone())
	{
		gAudiop->cleanupAudioSource(mAudioSourcep);
		mAudioSourcep = NULL;
	}

	if (mAudioSourcep && mAudioSourcep->isMuted() &&
		mAudioSourcep->getCurrentData() &&
		mAudioSourcep->getCurrentData()->getID() == audio_uuid)
	{
		LL_DEBUGS("AttachedSound") << "Already having sound " << audio_uuid
								   << " as muted sound, ignoring." << LL_ENDL;
		return;
	}

	getAudioSource(owner_id);

	if (mAudioSourcep)
	{
		bool queue = flags & LL_SOUND_FLAG_QUEUE;
		mAudioGain = gain;
		mAudioSourcep->setGain(gain);
		mAudioSourcep->setLoop(flags & LL_SOUND_FLAG_LOOP);
		mAudioSourcep->setSyncMaster(flags & LL_SOUND_FLAG_SYNC_MASTER);
		mAudioSourcep->setSyncSlave(flags & LL_SOUND_FLAG_SYNC_SLAVE);
		mAudioSourcep->setQueueSounds(queue);
		if (!queue)
		{
			// Stop any current sound first to avoid "farts of doom" (SL-1541)
			// -MG
			mAudioSourcep->stop();
		}

		// Play this sound if region maturity permits
		if (gAgent.canAccessMaturityAtGlobal(getPositionGlobal()))
		{
			LL_DEBUGS("AttachedSound") << "Playing attached sound: "
									   << audio_uuid << LL_ENDL;
			// Check cutoff radius in case this update was an object-update
			// with a new value
			mAudioSourcep->checkCutOffRadius();
			mAudioSourcep->play(audio_uuid);
		}
	}
}

LLAudioSource* LLViewerObject::getAudioSource(const LLUUID& owner_id)
{
	if (!mAudioSourcep)
	{
		// Arbitrary low gain for a sound that is not playing. This is used for
		// sound preloads, for example.
		mAudioSourcep = new LLAudioSourceVO(mID, owner_id, 0.01f, this);
		if (gAudiop)
		{
			gAudiop->addAudioSource(mAudioSourcep);
		}
	}

	return mAudioSourcep;
}

void LLViewerObject::adjustAudioGain(F32 gain)
{
	if (gAudiop && mAudioSourcep)
	{
		mAudioGain = gain;
		mAudioSourcep->setGain(mAudioGain);
	}
}

bool LLViewerObject::unpackParameterEntry(U16 param_type, LLDataPacker* dp)
{
	if (LLNetworkData::PARAMS_MESH == param_type)
	{
		param_type = LLNetworkData::PARAMS_SCULPT;
	}

	LLNetworkData* param = getExtraParameterEntryCreate(param_type);
	if (!param)
	{
		return false;
	}

	param->unpack(*dp);
	mExtraParameterInUse[LL_EPARAM_INDEX(param_type)] = true;
	parameterChanged(param_type, param, true, false);
	return true;
}

LLNetworkData* LLViewerObject::createNewParameterEntry(U16 param_type)
{
	LLNetworkData* new_block = NULL;
	switch (param_type)
	{
		case LLNetworkData::PARAMS_FLEXIBLE:
			new_block = new LLFlexibleObjectData();
			break;

		case LLNetworkData::PARAMS_LIGHT:
			new_block = new LLLightParams();
			break;

		case LLNetworkData::PARAMS_SCULPT:
			new_block = new LLSculptParams();
			break;

		case LLNetworkData::PARAMS_LIGHT_IMAGE:
			new_block = new LLLightImageParams();
			break;

		case LLNetworkData::PARAMS_EXTENDED_MESH:
			new_block = new LLExtendedMeshParams();
			break;

		case LLNetworkData::PARAMS_RENDER_MATERIAL:
			new_block = new LLRenderMaterialParams();
			break;

		case LLNetworkData::PARAMS_REFLECTION_PROBE:
			new_block = new LLReflectionProbeParams();
			break;

		default:
			llinfos << "Unknown param type #" << param_type << llendl;
	}
	if (!new_block) return NULL;

	S32 i = LL_EPARAM_INDEX(param_type);
	mExtraParameters[i] = new_block;
	mExtraParameterInUse[i] = false;	// Not yet in use
	return new_block;
}

LLNetworkData* LLViewerObject::getExtraParameterEntry(U16 param_type) const
{
	S32 i = LL_EPARAM_INDEX(param_type);
	return i >= 0 && i < LL_EPARAMS_COUNT ? mExtraParameters[i] : NULL;
}

LLNetworkData* LLViewerObject::getExtraParameterEntryCreate(U16 param_type)
{
	LLNetworkData* param = getExtraParameterEntry(param_type);
	if (!param)
	{
		param = createNewParameterEntry(param_type);
	}
	return param;
}

bool LLViewerObject::getParameterEntryInUse(U16 param_type) const
{
	S32 i = LL_EPARAM_INDEX(param_type);
	return i >= 0 && i < LL_EPARAMS_COUNT && mExtraParameterInUse[i];
}

LLFlexibleObjectData* LLViewerObject::getFlexibleObjectData() const
{
	constexpr S32 index = LL_EPARAM_INDEX(LLNetworkData::PARAMS_FLEXIBLE);
	if (mExtraParameterInUse[index])
	{
		return (LLFlexibleObjectData*)mExtraParameters[index];
	}
	return NULL;
}

LLLightParams* LLViewerObject::getLightParams() const
{
	constexpr S32 index = LL_EPARAM_INDEX(LLNetworkData::PARAMS_LIGHT);
	if (mExtraParameterInUse[index])
	{
		return (LLLightParams*)mExtraParameters[index];
	}
	return NULL;
}

LLSculptParams* LLViewerObject::getSculptParams() const
{
	constexpr S32 index = LL_EPARAM_INDEX(LLNetworkData::PARAMS_SCULPT);
	if (mExtraParameterInUse[index])
	{
		return (LLSculptParams*)mExtraParameters[index];
	}
	return NULL;
}

LLLightImageParams* LLViewerObject::getLightImageParams() const
{
	constexpr S32 index = LL_EPARAM_INDEX(LLNetworkData::PARAMS_LIGHT_IMAGE);
	if (mExtraParameterInUse[index])
	{
		return (LLLightImageParams*)mExtraParameters[index];
	}
	return NULL;
}

LLExtendedMeshParams* LLViewerObject::getExtendedMeshParams() const
{
	constexpr S32 index = LL_EPARAM_INDEX(LLNetworkData::PARAMS_EXTENDED_MESH);
	if (mExtraParameterInUse[index])
	{
		return (LLExtendedMeshParams*)mExtraParameters[index];
	}
	return NULL;
}

LLRenderMaterialParams* LLViewerObject::getMaterialRenderParams() const
{
	constexpr S32 idx = LL_EPARAM_INDEX(LLNetworkData::PARAMS_RENDER_MATERIAL);
	if (mExtraParameterInUse[idx])
	{
		return (LLRenderMaterialParams*)mExtraParameters[idx];
	}
	return NULL;
}

LLReflectionProbeParams* LLViewerObject::getReflectionProbeParams() const
{
	constexpr S32 idx = LL_EPARAM_INDEX(LLNetworkData::PARAMS_REFLECTION_PROBE);
	if (mExtraParameterInUse[idx])
	{
		return (LLReflectionProbeParams*)mExtraParameters[idx];
	}
	return NULL;
}

bool LLViewerObject::setParameterEntry(U16 param_type,
									   const LLNetworkData& new_value,
									   bool local_origin)
{
	LLNetworkData* paramp = getExtraParameterEntryCreate(param_type);
	if (!paramp)
	{
		return false;
	}
	bool& in_use = mExtraParameterInUse[LL_EPARAM_INDEX(param_type)];
	if (in_use && new_value == *paramp)
	{
		return false;
	}
	in_use = true;
	paramp->copy(new_value);
	parameterChanged(param_type, paramp, true, local_origin);
	return true;
}

// Assumed to be called locally. If in_use is true, will create a new extra
// parameter if none exists.
bool LLViewerObject::setParameterEntryInUse(U16 param_type, bool in_use,
											bool local_origin)
{
	LLNetworkData* paramp = getExtraParameterEntryCreate(param_type);
	if (!paramp)
	{
		return false;
	}
	bool& is_in_use = mExtraParameterInUse[LL_EPARAM_INDEX(param_type)];
	if (is_in_use != in_use)
	{
		is_in_use = in_use;
		parameterChanged(param_type, paramp, in_use, local_origin);
		return true;
	}
	return false;
}

//virtual
void LLViewerObject::parameterChanged(U16 param_type, bool local_origin)
{
	LLNetworkData* param = getExtraParameterEntry(param_type);
	if (param)
	{
		parameterChanged(param_type, param,
						 mExtraParameterInUse[LL_EPARAM_INDEX(param_type)],
						 local_origin);
	}
}

//virtual
void LLViewerObject::parameterChanged(U16 param_type, LLNetworkData* data,
									  bool in_use, bool local_origin)
{
	// Special treatment for render materials.
	if (param_type == LLNetworkData::PARAMS_RENDER_MATERIAL)
	{
		if (local_origin)
		{
			// Do not send the render material Id in this way as it will get
			// out-of-sync with other sent client data.
			// See LLViewerObject::setRenderMaterialID() and LLGLTFMaterialList
			llwarns << "Render materials shall not be updated on the server in this way."
					<< llendl;
		}
		else
		{
			const LLRenderMaterialParams* params =
				in_use ? getMaterialRenderParams() : NULL;
			setRenderMaterialIDs(params, false);
		}
		return;
	}

	if (!local_origin)
	{
		return;
	}

	LLViewerRegion* regionp = getRegion();
	if (!regionp)
	{
		return;
	}

	// Change happened on the viewer. Send the change up
	U8 tmp[MAX_OBJECT_PARAMS_SIZE];
	LLDataPackerBinaryBuffer dpb(tmp, MAX_OBJECT_PARAMS_SIZE);
	if (data->pack(dpb))
	{
		U32 datasize = (U32)dpb.getCurrentSize();

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ObjectExtraParams);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, mLocalID);

		msg->addU16Fast(_PREHASH_ParamType, param_type);
		msg->addBoolFast(_PREHASH_ParamInUse, in_use);

		msg->addU32Fast(_PREHASH_ParamSize, datasize);
		msg->addBinaryDataFast(_PREHASH_ParamData, tmp, datasize);

		msg->sendReliable(regionp->getHost());
	}
	else
	{
		llwarns << "Failed to send object extra parameters: " << param_type
				<< llendl;
	}
}

void LLViewerObject::setDrawableState(U32 state, bool recursive)
{
	if (mDrawable)
	{
		mDrawable->setState(state);
	}
	if (recursive)
	{
		for (child_list_t::iterator iter = mChildList.begin(),
									end = mChildList.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child)	// Paranoia
			{
				child->setDrawableState(state, recursive);
			}
		}
	}
}

void LLViewerObject::clearDrawableState(U32 state, bool recursive)
{
	if (mDrawable)
	{
		mDrawable->clearState(state);
	}
	if (recursive)
	{
		for (child_list_t::iterator iter = mChildList.begin(),
									end = mChildList.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child)	// Paranoia
			{
				child->clearDrawableState(state, recursive);
			}
		}
	}
}

bool LLViewerObject::isDrawableState(U32 state, bool recursive) const
{
	bool matches = false;
	if (mDrawable)
	{
		matches = mDrawable->isState(state);
	}
	if (recursive)
	{
		for (child_list_t::const_iterator iter = mChildList.begin();
			 (iter != mChildList.end()) && matches; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child)	// Paranoia
			{
				matches &= child->isDrawableState(state, recursive);
			}
		}
	}

	return matches;
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// RN: these functions assume a 2-level hierarchy
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// Owned by anyone ?
bool LLViewerObject::permAnyOwner() const
{
	if (isRootEdit())
	{
		return flagObjectAnyOwner();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permAnyOwner();
	}
}

// Owned by this viewer ?
bool LLViewerObject::permYouOwner() const
{
	if (isRootEdit())
	{
		return flagObjectYouOwner();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permYouOwner();
	}
}

// Owned by a group ?
bool LLViewerObject::permGroupOwner() const
{
	if (isRootEdit())
	{
		return flagObjectGroupOwned();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permGroupOwner();
	}
}

// Can the owner edit ?
bool LLViewerObject::permOwnerModify() const
{
	if (isRootEdit())
	{
		return flagObjectOwnerModify();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permOwnerModify();
	}
}

// Can edit ?
bool LLViewerObject::permModify() const
{
	if (isRootEdit())
	{
		return flagObjectModify();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permModify();
	}
}

// Can copy ?
bool LLViewerObject::permCopy() const
{
	if (isRootEdit())
	{
		return flagObjectCopy();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permCopy();
	}
}

// Can move ?
bool LLViewerObject::permMove() const
{
	if (isRootEdit())
	{
		return flagObjectMove();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permMove();
	}
}

// Can be transferred ?
bool LLViewerObject::permTransfer() const
{
	if (isRootEdit())
	{
		return flagObjectTransfer();
	}
	else
	{
		return ((LLViewerObject*)getParent())->permTransfer();
	}
}

// Can only open objects that you own, or that someone has
// given you modify rights to.  JC
bool LLViewerObject::allowOpen() const
{
	return !flagInventoryEmpty() && (permYouOwner() || permModify());
}

LLViewerObject::LLInventoryCallbackInfo::~LLInventoryCallbackInfo()
{
	if (mListener)
	{
		mListener->clearVOInventoryListener(mObject);
	}
}

bool LLViewerObject::recursiveSetMaxLOD(bool lock)
{
	LLViewerObject* rootp = getRootEdit();
	if (!rootp || rootp->mDead)
	{
		return false;
	}

	bool result = false;

	LLVOVolume* volp = rootp->asVolume();
	if (volp)
	{
		volp->setMaxLOD(lock);
		result = true;
	}

	for (child_list_t::iterator it = rootp->mChildList.begin(),
								end = rootp->mChildList.end();
		 it != end; ++it)
	{
		LLViewerObject* childp = *it;
		if (childp)
		{
			LLVOVolume* volp = childp->asVolume();
			if (volp)
			{
				volp->setMaxLOD(lock);
				result = true;
			}
		}
	}

	return result;
}

bool LLViewerObject::isLockedAtMaxLOD()
{
	LLViewerObject* rootp = getRootEdit();
	if (!rootp || rootp->mDead)
	{
		return false;
	}

	LLVOVolume* volp = rootp->asVolume();
	if (volp && volp->getMaxLOD())
	{
		return true;
	}

	for (child_list_t::iterator it = rootp->mChildList.begin(),
								end = rootp->mChildList.end();
		 it != end; ++it)
	{
		LLViewerObject* childp = *it;
		if (childp)
		{
			LLVOVolume* volp = childp->asVolume();
			if (volp && volp->getMaxLOD())
			{
				return true;
			}
		}
	}

	return false;
}

void LLViewerObject::updateVolume(const LLVolumeParams& volume_params)
{
	if (setVolume(volume_params, 1)) // *FIX: magic number, ack !
	{
		// Transmit the update to the simulator
		sendShapeUpdate();
		markForUpdate();
	}
}

void LLViewerObject::recursiveMarkForUpdate()
{
	if (mDrawable.notNull())
	{
		for (child_list_t::iterator it = mChildList.begin(),
									end = mChildList.end();
			 it != end; ++it)
		{
			LLViewerObject* child = *it;
			if (child)	// Paranoia
			{
				child->markForUpdate();
			}
		}
		markForUpdate();
	}
}

//virtual
void LLViewerObject::markForUpdate(bool rebuild_all)
{
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
		LLDrawable::EDrawableFlags flags = rebuild_all ?
			// *HACK: used to force-refresh the visibility of objects when they
			// are "missing" (this also should force an octree rebuild)... HB
			LLDrawable::REBUILD_ALL :
			LLDrawable::REBUILD_GEOMETRY;
		gPipeline.markRebuild(mDrawable, flags);
	}
}

bool LLViewerObject::isPermanentEnforced() const
{
	return flagObjectPermanent() && mRegionp != gAgent.getRegion() &&
		   !gAgent.isGodlike();
}

bool LLViewerObject::getIncludeInSearch() const
{
	return flagIncludeInSearch();
}

void LLViewerObject::setIncludeInSearch(bool include_in_search)
{
	setFlags(FLAGS_INCLUDE_IN_SEARCH, include_in_search);
}

void LLViewerObject::setRegion(LLViewerRegion* regionp)
{
	if (!regionp)
	{
		llwarns << "viewer object set region to NULL" << llendl;
	}
	if (regionp != mRegionp)
	{
		if (mRegionp)
		{
			mRegionp->removeFromCreatedList(getLocalID());
		}
		if (regionp)
		{
			regionp->addToCreatedList(getLocalID());
		}
	}

	mLatestRecvPacketID = 0;
	mRegionp = regionp;

	for (child_list_t::iterator i = mChildList.begin(), end = mChildList.end();
		 i != end; ++i)
	{
		LLViewerObject* child = *i;
		child->setRegion(regionp);
	}

	if (mPuppetAvatar)
	{
		mPuppetAvatar->setRegion(regionp);
	}

	setChanged(MOVED | SILHOUETTE);
	updateDrawable(false);
}

bool LLViewerObject::specialHoverCursor() const
{
	return mClickAction != 0 || flagUsePhysics() || flagHandleTouch();
}

void LLViewerObject::updateFlags(bool physics_changed)
{
	LLViewerRegion* regionp = getRegion();
	if (!regionp) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_ObjectFlagUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_ObjectLocalID, getLocalID());
	msg->addBoolFast(_PREHASH_UsePhysics, flagUsePhysics());
	msg->addBool(_PREHASH_IsTemporary, flagTemporaryOnRez());
	msg->addBool(_PREHASH_IsPhantom, flagPhantom());
	// Stinson 02/28/2012: this _PREHASH_CastsShadows bool is no longer used
	// in either the viewer or the simulator. The simulator code does not even
	// unpack this value when the message is received, but could be potentially
	// hijacked in the future for another use should the urgent need arise.
	msg->addBool(_PREHASH_CastsShadows, false);
	if (physics_changed)
	{
		msg->nextBlock(_PREHASH_ExtraPhysics);
		msg->addU8(_PREHASH_PhysicsShapeType, getPhysicsShapeType());
		msg->addF32(_PREHASH_Density, getPhysicsDensity());
		msg->addF32(_PREHASH_Friction, getPhysicsFriction());
		msg->addF32(_PREHASH_Restitution, getPhysicsRestitution());
		msg->addF32(_PREHASH_GravityMultiplier, getPhysicsGravity());
	}
	msg->sendReliable(regionp->getHost());

	if (physics_changed)
	{
		LLViewerObject* root = getRootEdit();
		root->mCostStale = true;
	}
}

bool LLViewerObject::setFlags(U32 flags, bool state)
{
	bool setit = setFlagsWithoutUpdate(flags, state);
#if 0	// BUG: Sometimes viewer physics and simulator physics get out of sync.
		// To fix this, always send update to simulator.
 	if (setit)
#endif
	{
		updateFlags();
	}
	return setit;
}

bool LLViewerObject::setFlagsWithoutUpdate(U32 flags, bool state)
{
	bool setit = false;
	if (state)
	{
		if ((mFlags & flags) != flags)
		{
			mFlags |= flags;
			setit = true;
		}
	}
	else if ((mFlags & flags) != 0)
	{
		mFlags &= ~flags;
		setit = true;
	}

	return setit;
}

void LLViewerObject::setPhysicsShapeType(U8 type)
{
	mPhysicsShapeUnknown = false;
	if (type != mPhysicsShapeType)
	{
		mPhysicsShapeType = type;
		mCostStale = true;
	}
}

void LLViewerObject::setPhysicsGravity(F32 gravity)
{
	mPhysicsGravity = gravity;
}

void LLViewerObject::setPhysicsFriction(F32 friction)
{
	mPhysicsFriction = friction;
}

void LLViewerObject::setPhysicsDensity(F32 density)
{
	mPhysicsDensity = density;
}

void LLViewerObject::setPhysicsRestitution(F32 restitution)
{
	mPhysicsRestitution = restitution;
}

U8 LLViewerObject::getPhysicsShapeType() const
{
	if (mPhysicsShapeUnknown)
	{
		mPhysicsShapeUnknown = false;
		gObjectList.updatePhysicsFlags(this);
	}

	return mPhysicsShapeType;
}

void LLViewerObject::applyAngularVelocity(F32 dt)
{
	// Do target omega here
	mRotTime += dt;
	LLVector3 ang_vel = getAngularVelocity();
	F32 omega = ang_vel.lengthSquared();
	F32 angle = 0.f;
	LLQuaternion dQ;
	if (omega > 0.00001f)
	{
		omega = sqrtf(omega);
		angle = omega * dt;

		ang_vel /= omega;

		// Calculate the delta increment based on the object's angular velocity
		dQ.setAngleAxis(angle, ang_vel);

		if (sUseNewTargetOmegaCode)
		{
			// Accumulate the angular velocity rotations to re-apply in the
			// case of an object update
			mAngularVelocityRot *= dQ;
		}

		// Just apply the delta increment to the current rotation
		setRotation(getRotation() * dQ);
		setChanged(MOVED | SILHOUETTE);
	}
}

void LLViewerObject::resetRot()
{
	mRotTime = 0.f;

	if (sUseNewTargetOmegaCode)
	{
		// Reset the accumulated angular velocity rotation
		mAngularVelocityRot.loadIdentity();
	}
}

bool LLViewerObject::isAnySelected() const
{
	if (mUserSelected)
	{
		return true;
	}

	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* child = *iter;
		if (child && child->mUserSelected)
		{
			return true;
		}
	}

	return false;
}

//virtual
void LLViewerObject::setSelected(bool sel)
{
	mUserSelected = sel;

	if (sUseNewTargetOmegaCode)
	{
		resetRot();
	}
	else
	{
		mRotTime = 0.f;
	}

	if (!sel)
	{
		setAllTESelected(false);
	}
}

//virtual
U32 LLViewerObject::getPartitionType() const
{
	return LLViewerRegion::PARTITION_NONE;
}

void LLViewerObject::dirtySpatialGroup() const
{
	if (mDrawable)
	{
		LLSpatialGroup* groupp = mDrawable->getSpatialGroup();
		if (groupp)
		{
			groupp->dirtyGeom();
			gPipeline.markRebuild(groupp);
		}
	}
}

//virtual
void LLViewerObject::dirtyMesh()
{
	if (mDrawable)
	{
		gPipeline.markRebuild(mDrawable);
#if 0
		LLSpatialGroup* group = mDrawable->getSpatialGroup();
		if (group)
		{
			group->dirtyMesh();
		}
#endif
	}
}

// virtual
void LLStaticViewerObject::updateDrawable(bool force_damped)
{
	// Force an immediate rebuild on any update
	if (mDrawable.notNull())
	{
		mDrawable->updateXform(true);
		gPipeline.markRebuild(mDrawable);
	}
	clearChanged(SHIFTED);
}

void LLViewerObject::saveUnselectedChildrenPosition(std::vector<LLVector3>& positions)
{
	if (mChildList.empty() || !positions.empty())
	{
		return;
	}

	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && !childp->mUserSelected && childp->mDrawable.notNull())
		{
			positions.emplace_back(childp->getPositionEdit());
		}
	}
}

void LLViewerObject::saveUnselectedChildrenRotation(std::vector<LLQuaternion>& rotations)
{
	if (mChildList.empty())
	{
		return;
	}

	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && !childp->mUserSelected && childp->mDrawable.notNull())
		{
			rotations.emplace_back(childp->getRotationEdit());
		}
	}
}

// counter-rotation
void LLViewerObject::resetChildrenRotationAndPosition(const std::vector<LLQuaternion>& rotations,
													  const std::vector<LLVector3>& positions)
{
	if (mChildList.empty())
	{
		return;
	}

	S32 index = 0;
	LLQuaternion inv_rotation = ~getRotationEdit();
	LLVector3 offset = getPositionEdit();
	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && !childp->mUserSelected && childp->mDrawable.notNull())
		{
			if (childp->isAvatar())
			{
				LLVector3 reset_pos = (positions[index] - offset) *
									  inv_rotation;
				LLQuaternion reset_rot = rotations[index] * inv_rotation;

				LLVOAvatar* av = (LLVOAvatar*)childp;
				av->mDrawable->mXform.setPosition(reset_pos);
				av->mDrawable->mXform.setRotation(reset_rot);

				av->mDrawable->getVObj()->setPositionLocal(reset_pos, true);
				av->mDrawable->getVObj()->setRotation(reset_rot, true);

				LLManip::rebuild(childp);
			}
			else // Not an avatar
			{
				childp->setRotation(rotations[index] * inv_rotation);
				childp->setPositionLocal((positions[index] - offset) *
										 inv_rotation);
				LLManip::rebuild(childp);
			}
			++index;
		}
	}
}

// counter-translation
void LLViewerObject::resetChildrenPosition(const LLVector3& offset,
										   bool simplified,
										   bool skip_avatar_child)
{
	if (mChildList.empty())
	{
		return;
	}

	LLVector3 child_offset;
	if (simplified) //translation only, rotation matrix does not change
	{
		child_offset = offset * ~getRotation();
	}
	else	// Rotation matrix might change too.
	{
		if (isAttachment() && mDrawable.notNull())
		{
			LLXform* attachment_point_xform = mDrawable->getXform()->getParent();
			LLQuaternion parent_rotation = getRotation() * attachment_point_xform->getWorldRotation();
			child_offset = offset * ~parent_rotation;
		}
		else
		{
			child_offset = offset * ~getRenderRotation();
		}
	}

	for (child_list_t::const_iterator iter = mChildList.begin(),
									  end = mChildList.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && !childp->mUserSelected && childp->mDrawable.notNull())
		{
			if (!childp->isAvatar())
			{
				childp->setPositionLocal(childp->getPosition() + child_offset);
				LLManip::rebuild(childp);
			}
			else if (!skip_avatar_child)
			{
				LLVOAvatar* av = (LLVOAvatar*)childp;
				LLVector3 reset_pos = child_offset +
									  av->mDrawable->mXform.getPosition();
				av->mDrawable->mXform.setPosition(reset_pos);
				av->mDrawable->getVObj()->setPositionLocal(reset_pos);

				LLManip::rebuild(childp);
			}
		}
	}
}

//static
void LLViewerObject::setUpdateInterpolationTimes(F32 interpolate_time,
												 F32 phase_out_time,
												 F32 region_interp_time)
{
	if (interpolate_time < 0.f || phase_out_time < 0.f ||
		phase_out_time > interpolate_time || region_interp_time < 0.5f ||
		region_interp_time > 5.f)
	{
		llwarns << "Invalid values for interpolation or phase out times, resetting to defaults"
				<< llendl;
		interpolate_time = 3.f;
		phase_out_time = 1.f;
		region_interp_time = 1.f;
	}
	sMaxUpdateInterpolationTime = (F64)interpolate_time;
	sPhaseOutUpdateInterpolationTime = (F64)phase_out_time;
	sMaxRegionCrossingInterpolationTime = (F64)region_interp_time;
}

const LLUUID& LLViewerObject::extractAttachmentItemID()
{
	LLUUID item_id;
	LLNameValue* item_id_nv = getNVPair("AttachItemID");
	if (item_id_nv)
	{
		const char* s = item_id_nv->getString();
		if (s)
		{
			item_id.set(s);
		}
	}
	setAttachmentItemID(item_id);
	return getAttachmentItemID();
}

const std::string& LLViewerObject::getAttachmentItemName()
{
	if (isAttachment())
	{
		LLInventoryItem* item = gInventory.getItem(getAttachmentItemID());
		if (item)
		{
			return item->getName();
		}
	}
	return LLStringUtil::null;
}

//virtual
LLVOAvatar* LLViewerObject::getAvatar() const
{
	LLVOAvatar* avatarp = (LLVOAvatar*)getPuppetAvatar();
	if (!avatarp && isAttachment())
	{
		LLViewerObject* vobj = (LLViewerObject*)getParent();

		while (vobj && !vobj->asAvatar())
		{
			vobj = (LLViewerObject*)vobj->getParent();
		}

		avatarp = (LLVOAvatar*)vobj;
	}

	return avatarp;
}

// If this object is directly or indirectly parented by an avatar, return it.
// Normally getAvatar() is the correct function to call and it will give the
// avatar used for skinning. The exception is with animated objects that are
// also attachments; in that case, getAvatar() will return the puppet avatar,
// used for skinning, and getAvatarAncestor() will return the avatar to which
// the object is attached.
LLVOAvatar* LLViewerObject::getAvatarAncestor()
{
	LLViewerObject* vobj = (LLViewerObject*)getParent();
	while (vobj)
	{
		LLVOAvatar* avatarp = vobj->asAvatar();
		if (avatarp)
		{
			return avatarp;
		}
		vobj = (LLViewerObject*)vobj->getParent();
	}

	return NULL;
}

bool LLViewerObject::isHiglightedOrBeacon() const
{
	static LLCachedControl<bool> beacons_always_on(gSavedSettings,
												   "BeaconAlwaysOn");
	if (!beacons_always_on && !LLPipeline::sRenderBeaconsFloaterOpen)
	{
		return false;
	}
	if (!LLPipeline::sRenderHighlight || !LLPipeline::highlightable(this))
	{
		return false;
	}
	bool is_scripted = flagScripted();
	return (is_scripted && LLPipeline::sRenderScriptedBeacons) ||
		   (is_scripted && flagHandleTouch() &&
			LLPipeline::sRenderScriptedTouchBeacons) ||
		   (isAudioSource() && LLPipeline::sRenderSoundBeacons) ||
		   (getMediaType() != MEDIA_NONE && LLPipeline::sRenderMOAPBeacons) ||
		   (isParticleSource() && LLPipeline::sRenderParticleBeacons) ||
		   (flagUsePhysics() && LLPipeline::sRenderPhysicalBeacons);
}

class ObjectPhysicsProperties final : public LLHTTPNode
{
public:
	void post(LLHTTPNode::ResponsePtr responder, const LLSD& context,
			  const LLSD& input) const override
	{
		LLSD object_data = input["body"]["ObjectData"];
		S32 num_entries = object_data.size();

		for (S32 i = 0; i < num_entries; ++i)
		{
			LLSD& curr_object_data = object_data[i];
			U32 local_id = curr_object_data["LocalID"].asInteger();

			// Iterate through nodes at end, since it can be on both the
			// regular AND hover list
			struct f : public LLSelectedNodeFunctor
			{
				LL_INLINE f(const U32& id)
				:	mID(id)
				{
				}

				bool LL_INLINE apply(LLSelectNode* node) override
				{
					return node->getObject() &&
						   node->getObject()->mLocalID == mID;
				}

				U32 mID;
			} fn(local_id);

			LLSelectNode* nodep = gSelectMgr.getSelection()->getFirstNode(&fn);
			if (!nodep)
			{
				continue;
			}
			LLViewerObject* objp = nodep->getObject();
			if (!objp || objp->isDead())
			{
				continue;
			}

			// The LLSD message builder does not know how to handle U8, so we
			// need to send as S8 and cast.
			U8 type = (U8)curr_object_data["PhysicsShapeType"].asInteger();
			F32 density = (F32)curr_object_data["Density"].asReal();
			F32 friction = (F32)curr_object_data["Friction"].asReal();
			F32 restitution = (F32)curr_object_data["Restitution"].asReal();
			F32 gravity = (F32)curr_object_data["GravityMultiplier"].asReal();
			objp->setPhysicsShapeType(type);
			objp->setPhysicsGravity(gravity);
			objp->setPhysicsFriction(friction);
			objp->setPhysicsDensity(density);
			objp->setPhysicsRestitution(restitution);
		}

		dialog_refresh_all();
	}
};

LLHTTPRegistration<ObjectPhysicsProperties>
	gHTTPRegistrationObjectPhysicsProperties("/message/ObjectPhysicsProperties");

#if LL_ANIMESH_VPARAMS
class LLExtendedAttributesDispatchHandler final
:	public LLDispatchHandler,
	public LLInitClass<LLExtendedAttributesDispatchHandler>
{
protected:
	LOG_CLASS(LLExtendedAttributesDispatchHandler);

public:
	static void initClass();

	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& object_id, const sparam_t& strings) override
	{
		LLSD message;
		sparam_t::const_iterator it = strings.begin();
		if (it != strings.end())
		{
			const std::string& llsd_raw = *it++;
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(message, llsd_data,
											llsd_raw.length()))
			{
				llwarns << "Invalid extended parameters data for object Id: "
						<< object_id << " - Data: " << llsd_raw << llendl;
				return true;
			}
		}
		LL_DEBUGS("Puppets") << "Handling extended attributes message for object Id "
							 << object_id << " - Data:\n"
							 << ll_pretty_print_sd(message) << LL_ENDL;
		LLObjectExtendedAttributesMap::setAttributes(object_id, message);

		// Process the parameters
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp && !objectp->isDead())
		{
			objectp->applyExtendedAttributes();
		}
		else
		{
			llwarns << "Extended attributes received for unknown or dead object: "
					<< object_id << llendl;
		}

		return true;
	}
};
LLExtendedAttributesDispatchHandler extended_attributes_dispatch_handler;

//static
void LLExtendedAttributesDispatchHandler::initClass()
{
	if (!gGenericDispatcher.isHandlerPresent("ObjectExtendedAttributes"))
	{
		gGenericDispatcher.addHandler("ObjectExtendedAttributes",
									  &extended_attributes_dispatch_handler);
	}
}

LLSD LLViewerObject::getVisualParamsSD() const
{
	LLSD params_sd = LLObjectExtendedAttributesMap::getField(mID,
															 "VisualParams");
	return params_sd.isMap() ? params_sd : LLSD();
}

void LLViewerObject::applyExtendedAttributes()
{
	if (isDead()) return;

	LLSD params_sd = LLObjectExtendedAttributesMap::getField(mID,
															 "VisualParams");
	if (!params_sd.isMap())
	{
		LL_DEBUGS("Puppets") << "Map does not have suitable data for VisualParams:\n"
							 << ll_pretty_print_sd(LLObjectExtendedAttributesMap::getData(mID))
							 << LL_ENDL;
		return;
	}

	LL_DEBUGS("Puppets") << "Processing visual params for object Id " << mID
						 << LL_ENDL;

	LLVOVolume* volp = asVolume();
	if (!volp || !volp->isAnimatedObject())
	{
		llwarns << "Ignoring visual params state for non-"
				<< (volp ? "animated" : "volume") << " object " << mID
				<< llendl;
		return;
	}

	LLVOAvatarPuppet* puppetp = volp->getPuppetAvatar();
	if (!puppetp)
	{
		llwarns << "Puppet avatar not found for object Id: " << mID << llendl;
		return;
	}

	// Copy into std::map so we can traverse keys in sorted order.
	std::map<S32, F32> param_vals_map;
	for (LLSD::map_iterator it = params_sd.beginMap(),
							end = params_sd.endMap();
		 it != end; ++it)
	{
		const std::string& param_id_str = it->first;
		S32 param_id = boost::lexical_cast<S32>(param_id_str);
		F32 normalized_weight = it->second.asReal();
		param_vals_map[param_id] = normalized_weight;
	}
	bool params_changed = false;
	for (std::map<S32, F32>::iterator it = param_vals_map.begin(),
									  end = param_vals_map.end();
		 it != end; ++it)
	{
		S32 param_id = it->first;
		F32 normalized_weight = it->second;
		LLVisualParam* paramp = puppetp->getVisualParam(param_id);
		if (!paramp)
		{
			llwarns << "Visual param not found for id: " << param_id
					<< " - Object: " << mID << llendl;
			continue;
		}
		F32 weight = lerp(paramp->getMinWeight(), paramp->getMaxWeight(),
						  normalized_weight);
		if (paramp->getWeight() != weight)
		{
			paramp->setWeight(weight, false);
			params_changed = true;
		}
	}
	if (params_changed)
	{
		puppetp->updateVisualParams();
	}
	if (gShowObjectUpdates && !param_vals_map.empty())
	{
		gPipeline.addDebugBlip(getPositionAgent(), LLColor4::magenta);
	}
}
#endif
