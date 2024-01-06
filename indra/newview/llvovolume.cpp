/**
 * @file llvovolume.cpp
 * @brief LLVOVolume class implementation
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

// A "volume" is a box, cylinder, sphere, or other primitive shape.

#include "llviewerprecompiledheaders.h"

#include <sstream>

#include "llvovolume.h"

#include "imageids.h"
#include "llanimationstates.h"
#include "llavatarappearancedefines.h"
#include "lldir.h"
#include "hbfastmap.h"
#include "llfasttimer.h"
#include "llmaterialtable.h"
#include "llmatrix4a.h"
#include "llmediaentry.h"
#include "llpluginclassmedia.h"		// For code in the mediaEvent handler
#include "llprimitive.h"
#include "llsdutil.h"
#include "llvolume.h"
#include "llvolumemessage.h"
#include "llvolumemgr.h"
#include "llvolumeoctree.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llflexibleobject.h"
#include "llfloatertools.h"
#include "llgltfmateriallist.h"
#include "llhudmanager.h"
#include "llmaterialmgr.h"
#include "llmediadataclient.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "lltexturefetch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gCubeSnapshot
#include "llviewermediafocus.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llworld.h"

using namespace LLAvatarAppearanceDefines;

constexpr F32 FORCE_SIMPLE_RENDER_AREA = 512.f;
constexpr F32 FORCE_CULL_AREA = 8.f;

bool LLVOVolume::sAnimateTextures = true;
U32 LLVOVolume::sRenderMaxVBOSize = 4096;
F32 LLVOVolume::sLODFactor = 1.f;
F32 LLVOVolume::sDistanceFactor = 1.f;
S32 LLVOVolume::sNumLODChanges = 0;
#if 0	// Not yet implemented
S32 LLVOVolume::mRenderComplexity_last = 0;
S32 LLVOVolume::mRenderComplexity_current = 0;
#endif
LLPointer<LLObjectMediaDataClient> LLVOVolume::sObjectMediaClient = NULL;
LLPointer<LLObjectMediaNavigateClient> LLVOVolume::sObjectMediaNavigateClient = NULL;

constexpr U32 MAX_FACE_COUNT = 4096U;
S32 LLVolumeGeometryManager::sInstanceCount = 0;
LLFace** LLVolumeGeometryManager::sFullbrightFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sBumpFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sSimpleFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sNormFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sSpecFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sNormSpecFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sPbrFaces[2] = { NULL };
LLFace** LLVolumeGeometryManager::sAlphaFaces[2] = { NULL };

// Implementation class of LLMediaDataClientObject. See llmediadataclient.h
class LLMediaDataClientObjectImpl final : public LLMediaDataClientObject
{
public:
	LLMediaDataClientObjectImpl(LLVOVolume* obj, bool isNew)
	:	mObject(obj),
		mNew(isNew)
	{
		mObject->addMDCImpl();
	}

	~LLMediaDataClientObjectImpl() override
	{
		mObject->removeMDCImpl();
	}

	LL_INLINE U8 getMediaDataCount() const override
	{
		return mObject->getNumTEs();
	}

	LLSD getMediaDataLLSD(U8 index) const override
	{
		LLSD result;
		LLTextureEntry* tep = mObject->getTE(index);
		if (tep)
		{
			llassert((tep->getMediaData() != NULL) == tep->hasMedia());
			if (tep->getMediaData())
			{
				result = tep->getMediaData()->asLLSD();
				// *HACK: workaround bug in asLLSD() where whitelist is not set
				// properly. See DEV-41949
				if (!result.has(LLMediaEntry::WHITELIST_KEY))
				{
					result[LLMediaEntry::WHITELIST_KEY] = LLSD::emptyArray();
				}
			}
		}
		return result;
	}

	bool isCurrentMediaUrl(U8 index, const std::string& url) const override
	{
		LLTextureEntry* tep = mObject->getTE(index);
		if (tep && tep->getMediaData())
		{
			return tep->getMediaData()->getCurrentURL() == url;
		}
		return url.empty();
	}

	LL_INLINE LLUUID getID() const override		{ return mObject->getID(); }

	LL_INLINE void mediaNavigateBounceBack(U8 index) override
	{
		mObject->mediaNavigateBounceBack(index);
	}

	LL_INLINE bool hasMedia() const override
	{
		return mObject->hasMedia();
	}

	LL_INLINE void updateObjectMediaData(LLSD const& data,
										 const std::string& ver) override
	{
		mObject->updateObjectMediaData(data, ver);
	}

	F64 getMediaInterest() const override
	{
		F64 interest = mObject->getTotalMediaInterest();
		if (interest < 0.0)
		{
			// media interest not valid yet, try pixel area
			interest = mObject->getPixelArea();
			// *HACK: force recalculation of pixel area if interest is the
			// "magic default" of 1024.
			if (interest == 1024.0)
			{
				const_cast<LLVOVolume*>((LLVOVolume*)mObject)->setPixelAreaAndAngle();
				interest = mObject->getPixelArea();
			}
		}
		return interest;
	}

	LL_INLINE bool isInterestingEnough() const override
	{
		return LLViewerMedia::isInterestingEnough(mObject, getMediaInterest());
	}

	LL_INLINE const std::string& getCapabilityUrl(const char* name) const override
	{
		return mObject->getRegion()->getCapability(name);
	}

	LL_INLINE bool isDead() const override		{ return mObject->isDead(); }

	LL_INLINE U32 getMediaVersion() const override
	{
		return LLTextureEntry::getVersionFromMediaVersionString(mObject->getMediaURL());
	}

	LL_INLINE bool isNew() const override		{ return mNew; }

private:
	LLPointer<LLVOVolume> mObject;
	bool mNew;
};

///////////////////////////////////////////////////////////////////////////////
// LLVOVolume class
///////////////////////////////////////////////////////////////////////////////

LLVOVolume::LLVOVolume(const LLUUID& id, LLViewerRegion* regionp)
:	LLViewerObject(id, LL_PCODE_VOLUME, regionp),
	mVolumeImpl(NULL),
	mTextureAnimp(NULL),
	mTexAnimMode(0),
	mVObjRadius(LLVector3(1.f, 1.f, 0.5f).length()),
	mLastDistance(0.f),
	mLOD(0),
	mLockMaxLOD(false),
	mLODChanged(false),
	mVolumeChanged(false),
	mSculptChanged(false),
	mInMeshCache(false),
	mInSkinCache(false),
	mSkinInfoFailed(false),
	mColorChanged(false),
	mFaceMappingChanged(false),
	mServerDrawableUpdateCount(0),
	mLastServerDrawableUpdate(0.f),
	mSpotLightPriority(0.f),
	mLastFetchedMediaVersion(-1),
	mMDCImplCount(0),
	mLastRiggingInfoLOD(-1)
{
	mRelativeXform.setIdentity();
	mRelativeXformInvTrans.setIdentity();

	mNumFaces = 0;

	mMediaImplList.resize(getNumTEs());

	memset(&mIndexInTex, 0,
		   sizeof(S32) * LLRender::NUM_VOLUME_TEXTURE_CHANNELS);
}

LLVOVolume::~LLVOVolume()
{
	if (mTextureAnimp)
	{
		delete mTextureAnimp;
		mTextureAnimp = NULL;
	}

	if (mVolumeImpl)
	{
		delete mVolumeImpl;
		mVolumeImpl = NULL;
	}

	if (!mMediaImplList.empty())
	{
		for (U32 i = 0, count = mMediaImplList.size(); i < count; ++i)
		{
			if (mMediaImplList[i].notNull())
			{
				mMediaImplList[i]->removeObject(this);
			}
		}
	}

	mCostData = NULL;
}

void LLVOVolume::markDead()
{
	if (mDead)
	{
		return;
	}

	// Only call unregisterVolume() when 'this' actually got registered in the
	// mesh repository, else do not loose our time !  HB
	if (mInMeshCache || mInSkinCache)
	{
		gMeshRepo.unregisterVolume(this, mInMeshCache, mInSkinCache);
	}

	if (mMDCImplCount > 0 &&
		(sObjectMediaClient || sObjectMediaNavigateClient))
	{
		LLMediaDataClientObject::ptr_t obj =
			new LLMediaDataClientObjectImpl(const_cast<LLVOVolume*>(this),
											false);
		if (sObjectMediaClient)
		{
			sObjectMediaClient->removeFromQueue(obj);
		}
		if (sObjectMediaNavigateClient)
		{
			sObjectMediaNavigateClient->removeFromQueue(obj);
		}
	}

	// Detach all media impls from this object
	for (U32 i = 0, count = mMediaImplList.size(); i < count; ++i)
	{
		removeMediaImpl(i);
	}

	if (mSculptTexture.notNull())
	{
		mSculptTexture->removeVolume(LLRender::SCULPT_TEX, this);
	}

	if (mLightTexture.notNull())
	{
		mLightTexture->removeVolume(LLRender::LIGHT_TEX, this);
	}

	LLViewerObject::markDead();
}

//static
void LLVOVolume::initClass()
{
	updateSettings();
	initSharedMedia();
}

//static
void LLVOVolume::updateSettings()
{
	sRenderMaxVBOSize = llmin(gSavedSettings.getU32("RenderMaxVBOSize"), 32U);
	sLODFactor = llclamp(gSavedSettings.getF32("RenderVolumeLODFactor"), 0.1f,
						 9.f);
	sDistanceFactor = 1.f - 0.1f * sLODFactor;
}

//static
void LLVOVolume::initSharedMedia()
{
	// gSavedSettings better be around
	if (gSavedSettings.getBool("EnableStreamingMedia") &&
		gSavedSettings.getBool("PrimMediaMasterEnabled"))
	{
		F32 queue_delay = gSavedSettings.getF32("PrimMediaRequestQueueDelay");
		F32 retry_delay = gSavedSettings.getF32("PrimMediaRetryTimerDelay");
		U32 max_retries = gSavedSettings.getU32("PrimMediaMaxRetries");
		U32 sorted_size = gSavedSettings.getU32("PrimMediaMaxSortedQueueSize");
		U32 rr_size = gSavedSettings.getU32("PrimMediaMaxRoundRobinQueueSize");
		sObjectMediaClient = new LLObjectMediaDataClient(queue_delay,
														 retry_delay,
														 max_retries,
														 sorted_size, rr_size);
		sObjectMediaNavigateClient =
			new LLObjectMediaNavigateClient(queue_delay, retry_delay,
											max_retries, sorted_size, rr_size);
	}
	else
	{
		// Make sure all shared media are unloaded
		LLViewerMedia::setAllMediaEnabled(false, false);
		// Make sure the media clients will not be called uselessly
		sObjectMediaClient = NULL;
		sObjectMediaNavigateClient = NULL;
	}
}

//static
void LLVOVolume::cleanupClass()
{
	sObjectMediaClient = NULL;
	sObjectMediaNavigateClient = NULL;
	llinfos << "Number of LOD cache hits: " << LLVolume::sLODCacheHit
			<< " - Cache misses: " << LLVolume::sLODCacheMiss << llendl;
}

U32 LLVOVolume::processUpdateMessage(LLMessageSystem* mesgsys,
									 void** user_data, U32 block_num,
									 EObjectUpdateType update_type,
									 LLDataPacker* dp)
{
	static LLCachedControl<bool> kill_bogus_objects(gSavedSettings,
													"KillBogusObjects");

	bool old_volume_changed = mVolumeChanged;
	bool old_mapping_changed = mFaceMappingChanged;
	bool old_color_changed = mColorChanged;

	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(mesgsys, user_data,
													  block_num, update_type,
													  dp);

	LLUUID sculpt_id;
	U8 sculpt_type = 0;
	if (isSculpted())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			sculpt_id = sculpt_params->getSculptTexture();
			sculpt_type = sculpt_params->getSculptType();
		}
	}

	if (!dp)
	{
		if (update_type == OUT_FULL)
		{
			////////////////////////////////
			// Unpack texture animation data

			if (mesgsys->getSizeFast(_PREHASH_ObjectData, block_num,
									 _PREHASH_TextureAnim))
			{
				if (!mTextureAnimp)
				{
					mTextureAnimp = new LLViewerTextureAnim(this);
				}
				else if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
				{
					mTextureAnimp->reset();
				}
				mTexAnimMode = 0;

				mTextureAnimp->unpackTAMessage(mesgsys, block_num);
			}
			else if (mTextureAnimp)
			{
				delete mTextureAnimp;
				mTextureAnimp = NULL;
				for (S32 i = 0, count = getNumTEs(); i < count; ++i)
				{
					LLFace* facep = mDrawable->getFace(i);
					if (facep && facep->mTextureMatrix)
					{
						delete facep->mTextureMatrix;
						facep->mTextureMatrix = NULL;
					}
				}
				gPipeline.markTextured(mDrawable);
				mFaceMappingChanged = true;
				mTexAnimMode = 0;
			}

			// Unpack volume data
			LLVolumeParams volume_params;
			bool success =
				LLVolumeMessage::unpackVolumeParams(&volume_params, mesgsys,
													_PREHASH_ObjectData,
													block_num);
			if (!success)
			{
				llwarns_once << "Bogus volume parameters in object " << getID()
							 << " at " << getPositionRegion() << " owned by "
							 << mOwnerID << llendl;
				if (mRegionp)
				{
					// Do not cache this bogus object
					mRegionp->addCacheMissFull(getLocalID());
				}
				if (kill_bogus_objects)
				{
					LLViewerObjectList::sBlackListedObjects.emplace(getID());
					gObjectList.killObject(this);
					return INVALID_UPDATE;
				}
			}

			volume_params.setSculptID(sculpt_id, sculpt_type);

			if (setVolume(volume_params, 0))
			{
				markForUpdate();
			}
		}

		// Sigh, this needs to be done AFTER the volume is set as well,
		// otherwise bad stuff happens...
		////////////////////////////
		// Unpack texture entry data
		S32 result = unpackTEMessage(mesgsys, _PREHASH_ObjectData,
									 (S32)block_num);
		if (result == TEM_INVALID)
		{
			llwarns_once << "Bogus TE data in object " << getID() << " at "
						 << getPositionRegion() << " owned by " << mOwnerID
						 << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}
		if (result & TEM_CHANGE_MEDIA)
		{
			retval |= MEDIA_FLAGS_CHANGED;
		}
	}
	else if (update_type != OUT_TERSE_IMPROVED)
	{
		LLVolumeParams volume_params;
		bool success = LLVolumeMessage::unpackVolumeParams(&volume_params,
														   *dp);
		if (!success)
		{
			llwarns_once << "Bogus volume parameters in object " << getID()
						 << " at " << getPositionRegion() << " owned by "
						 << mOwnerID << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}

		volume_params.setSculptID(sculpt_id, sculpt_type);

		if (setVolume(volume_params, 0))
		{
			markForUpdate();
		}

		S32 result = unpackTEMessage(*dp);
		if (result == TEM_INVALID)
		{
			llwarns_once << "Bogus TE data in object " << getID() << " at "
						 << getPositionRegion() << " owned by " << mOwnerID
						 << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}
		else if (result & TEM_CHANGE_MEDIA)
		{
			retval |= MEDIA_FLAGS_CHANGED;
		}

		U32 value = dp->getPassFlags();
		if (value & 0x40)
		{
			if (!mTextureAnimp)
			{
				mTextureAnimp = new LLViewerTextureAnim(this);
			}
			else if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
			{
				mTextureAnimp->reset();
			}
			mTexAnimMode = 0;
			mTextureAnimp->unpackTAMessage(*dp);
		}
		else if (mTextureAnimp)
		{
			delete mTextureAnimp;
			mTextureAnimp = NULL;
			for (S32 i = 0, count = getNumTEs(); i < count; ++i)
			{
				LLFace* facep = mDrawable->getFace(i);
				if (facep && facep->mTextureMatrix)
				{
					delete facep->mTextureMatrix;
					facep->mTextureMatrix = NULL;
				}
			}
			gPipeline.markTextured(mDrawable);
			mFaceMappingChanged = true;
			mTexAnimMode = 0;
		}
		if (value & 0x400)
		{
			// Particle system (new)
			unpackParticleSource(*dp, mOwnerID, false);
		}
	}
	else
	{
		S32 texture_length = mesgsys->getSizeFast(_PREHASH_ObjectData,
												  block_num,
												  _PREHASH_TextureEntry);
		if (texture_length)
		{
			U8 tdpbuffer[1024];
			LLDataPackerBinaryBuffer tdp(tdpbuffer, 1024);
			mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
									   _PREHASH_TextureEntry,
									   tdpbuffer, 0, block_num, 1024);
			S32 result = unpackTEMessage(tdp);
			if (result & TEM_CHANGE_MEDIA)
			{
				retval |= MEDIA_FLAGS_CHANGED;
			}
			// On the fly TE updates break batches: isolate in octree.
			if (result &
				(TEM_CHANGE_TEXTURE | TEM_CHANGE_COLOR | TEM_CHANGE_MEDIA))
			{
				shrinkWrap();
			}
		}
	}

	if (retval & (MEDIA_URL_REMOVED | MEDIA_URL_ADDED | MEDIA_URL_UPDATED |
				  MEDIA_FLAGS_CHANGED))
	{
		// If only the media URL changed, and it is not a media version URL,
		// ignore it
		if (!((retval & (MEDIA_URL_ADDED | MEDIA_URL_UPDATED)) &&
			mMedia && !mMedia->mMediaURL.empty() &&
			!LLTextureEntry::isMediaVersionString(mMedia->mMediaURL)))
		{
			// If the media changed at all, request new media data
			LL_DEBUGS("MediaOnAPrim") << "Media update: " << getID()
									  << ": retval=" << retval << " Media URL: "
									  << (mMedia ?  mMedia->mMediaURL : std::string(""))
									  << LL_ENDL;
			requestMediaDataUpdate(retval & MEDIA_FLAGS_CHANGED);
		}
		else
		{
			llinfos << "Ignoring media update for: " << getID()
					<< " Media URL: "
					<< (mMedia ?  mMedia->mMediaURL : std::string(""))
					<< llendl;
		}
	}
	// ... and clean up any media impls
	cleanUpMediaImpls();

	if (!mLODChanged &&
		((!old_volume_changed && mVolumeChanged) ||
		 (!old_mapping_changed && mFaceMappingChanged) ||
		 (!old_color_changed && mColorChanged)))
	{
		onDrawableUpdateFromServer();
	}

	return retval;
}

void LLVOVolume::onDrawableUpdateFromServer()
{
	constexpr U32 UPDATES_UNTIL_ACTIVE = 8;
	constexpr F32 UPDATES_COUNT_TIMEOUT = 60.f;
	if (mDrawable.isNull() || mDrawable->isActive())
	{
		return;	// Nothing to do
	}
	if (gFrameTimeSeconds > mLastServerDrawableUpdate + UPDATES_COUNT_TIMEOUT)
	{
		// Reset the count to 1 since there has not been an update in a
		// while. HB
		mServerDrawableUpdateCount = 1;
	}
	else if (++mServerDrawableUpdateCount > UPDATES_UNTIL_ACTIVE)
	{
		LL_DEBUGS("DrawableUpdates") << "Making " << getID() << " active."
									 << LL_ENDL;
		mDrawable->makeActive();
	}
	mLastServerDrawableUpdate = gFrameTimeSeconds;
}

void LLVOVolume::animateTextures()
{
	if (mDead || !mTextureAnimp) return;

	// Animated texture break batches: isolate in octree.
	shrinkWrap();

	F32 off_s = 0.f, off_t = 0.f, scale_s = 1.f, scale_t = 1.f, rot = 0.f;
	S32 result = mTextureAnimp->animateTextures(off_s, off_t, scale_s, scale_t,
												rot);
	if (result)
	{
		if (!mTexAnimMode)
		{
			mFaceMappingChanged = true;
			gPipeline.markTextured(mDrawable);
		}
		mTexAnimMode = result | mTextureAnimp->mMode;

		S32 start = 0, end = mDrawable->getNumFaces() - 1;
		if (mTextureAnimp->mFace >= 0 && mTextureAnimp->mFace <= end)
		{
			start = end = mTextureAnimp->mFace;
		}

		LLVector3 trans, scale;
		LLMatrix4a scale_mat, tex_mat;
		static const LLVector3 translation(-0.5f, -0.5f, 0.f);
		for (S32 i = start; i <= end; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (!facep ||
				(facep->getVirtualSize() < MIN_TEX_ANIM_SIZE &&
				 facep->mTextureMatrix))
			{
				continue;
			}

			const LLTextureEntry* tep = facep->getTextureEntry();
			if (!tep)
			{
				continue;
			}

			if (!facep->mTextureMatrix)
			{
				facep->mTextureMatrix = new LLMatrix4();
			}

			if (!(result & LLViewerTextureAnim::ROTATE))
			{
				tep->getRotation(&rot);
			}

			if (!(result & LLViewerTextureAnim::TRANSLATE))
			{
				tep->getOffset(&off_s, &off_t);
			}
			trans.set(LLVector3(off_s + 0.5f, off_t + 0.5f, 0.f));

			if (!(result & LLViewerTextureAnim::SCALE))
			{
				tep->getScale(&scale_s, &scale_t);
			}
			scale.set(scale_s, scale_t, 1.f);

			tex_mat.setIdentity();
			tex_mat.translateAffine(translation);

			static const LLVector4a z_neg_axis(0.f, 0.f, -1.f);
			tex_mat.setMul(gl_gen_rot(rot * RAD_TO_DEG, z_neg_axis), tex_mat);

			scale_mat.setIdentity();
			scale_mat.applyScaleAffine(scale);
			tex_mat.setMul(scale_mat, tex_mat);	// Left mul

			tex_mat.translateAffine(trans);

			facep->mTextureMatrix->set(tex_mat.getF32ptr());
		}
	}
	else if (mTexAnimMode && mTextureAnimp->mRate == 0)
	{
		U8 start, count;
		if (mTextureAnimp->mFace == -1)
		{
			start = 0;
			count = getNumTEs();
		}
		else
		{
			start = (U8)mTextureAnimp->mFace;
			count = 1;
		}

		for (S32 i = start; i < start + count; ++i)
		{
			if (mTexAnimMode & LLViewerTextureAnim::TRANSLATE)
			{
				setTEOffset(i, mTextureAnimp->mOffS, mTextureAnimp->mOffT);
			}
			if (mTexAnimMode & LLViewerTextureAnim::SCALE)
			{
				setTEScale(i, mTextureAnimp->mScaleS, mTextureAnimp->mScaleT);
			}
			if (mTexAnimMode & LLViewerTextureAnim::ROTATE)
			{
				setTERotation(i, mTextureAnimp->mRot);
			}
		}

		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
		mTexAnimMode = 0;
	}
}

void LLVOVolume::updateTextures()
{
	constexpr F32 TEXTURE_AREA_REFRESH_TIME = 5.f; // seconds
	if (mTextureUpdateTimer.getElapsedTimeF32() < TEXTURE_AREA_REFRESH_TIME)
	{
		return;
	}

	updateTextureVirtualSize();

	static LLCachedControl<bool> destroy(gSavedSettings,
										 "DestroyUnseenVolumeVB");
	if (!destroy || mDrawable.isNull() || isVisible() || mDrawable->isActive())
	{
		return;
	}

	// Delete vertex buffer to free up some VRAM
	LLSpatialGroup* groupp = mDrawable->getSpatialGroup();
	if (groupp &&
		(groupp->mVertexBuffer.notNull() || !groupp->mBufferMap.empty() ||
		 !groupp->mDrawMap.empty()))
	{
		groupp->destroyGL(true);

		// Flag the group as having changed geometry so it gets a rebuild next
		// time it becomes visible
		groupp->setState(LLSpatialGroup::GEOM_DIRTY |
						 LLSpatialGroup::MESH_DIRTY |
						 LLSpatialGroup::NEW_DRAWINFO);
	}
}

bool LLVOVolume::isVisible() const
{
	if (mDrawable.notNull() && mDrawable->isVisible())
	{
		return true;
	}
#if 0
	if (isHUDAttachment())
	{
		return true;
	}
#endif
	if (isAttachment())
	{
		LLViewerObject* objp = (LLViewerObject*)getParent();
		while (objp && !objp->isAvatar())
		{
			objp = (LLViewerObject*)objp->getParent();
		}

		return objp && objp->mDrawable.notNull() &&
			   objp->mDrawable->isVisible();
	}

	return false;
}

// Updates the pixel area of all faces
void LLVOVolume::updateTextureVirtualSize(bool forced)
{
	if (mDrawable.isNull() || gCubeSnapshot)
	{
		return;
	}

	if (!forced)
	{
		if (!isVisible())
		{
			// Do not load textures for non-visible faces
			for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
			{
				LLFace* face = mDrawable->getFace(i);
				if (face)
				{
					face->setPixelArea(0.f);
					face->setVirtualSize(0.f);
				}
			}
			return;
		}

		if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SIMPLE))
		{
			return;
		}
	}

	if (LLViewerTexture::sDontLoadVolumeTextures ||
		gTextureFetchp->mDebugPause)
	{
		return;
	}

	mTextureUpdateTimer.reset();

	F32 old_area = mPixelArea;
	mPixelArea = 0.f;

	const S32 num_faces = mDrawable->getNumFaces();
	F32 min_vsize = 999999999.f, max_vsize = 0.f;
	bool is_ours = permYouOwner();
	bool is_hud = isHUDAttachment();
	bool debug_tex_area =
		gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA);
	bool debug_tex_prio =
		gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY);
	bool debug_face_alpha =
		gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_FACE_AREA);
	for (S32 i = 0; i < num_faces; ++i)
	{
		LLFace* face = mDrawable->getFace(i);
		if (!face) continue;

		const LLTextureEntry* tep = face->getTextureEntry();
		LLViewerTexture* imagep = face->getTexture();
		if (!imagep || !tep || face->mExtents[0].equals3(face->mExtents[1]))
		{
			continue;
		}

		F32 vsize;
		F32 old_size = face->getVirtualSize();

		if (is_hud)
		{
			// Rez our attachments faster and at full details !
			imagep->setBoostLevel(LLGLTexture::BOOST_HUD);
			// ... and do not discard our attachments textures
			imagep->dontDiscard();
			// Treat as full screen
			vsize = (F32)gViewerCamera.getScreenPixelArea();
			face->setPixelArea(vsize);
		}
		else
		{
			vsize = face->getTextureVirtualSize();
			// Rez our attachments faster and at full details !
			if (is_ours && isAttachment())
			{
				imagep->setBoostLevel(LLGLTexture::BOOST_HUD);
				// ... and do not discard our attachments textures
				imagep->dontDiscard();
			}
		}

		mPixelArea = llmax(mPixelArea, face->getPixelArea());

		if (face->mTextureMatrix)
		{
			// Animating textures also rez badly in Snowglobe because the
			// actual displayed area is only a fraction (corresponding to one
			// frame) of the animating texture. Let's fix that here. HB
			if (mTextureAnimp && mTextureAnimp->mScaleS > 0.f &&
				mTextureAnimp->mScaleT > 0.f)
			{
				// Adjust to take into account the actual frame size which is
				// only a portion of the animating texture.
				vsize /= mTextureAnimp->mScaleS / mTextureAnimp->mScaleT;
			}

			// If the face has gotten small enough to turn off texture
			// animation and texture animation is running, rebuild the render
			// batch for this face to turn off texture animation.
			if ((vsize < MIN_TEX_ANIM_SIZE && old_size >= MIN_TEX_ANIM_SIZE) ||
				(vsize >= MIN_TEX_ANIM_SIZE && old_size < MIN_TEX_ANIM_SIZE))
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
			}
		}

		face->setVirtualSize(vsize);
		if (!is_hud)
		{
			imagep->addTextureStats(vsize);
		}

		if (debug_tex_area)
		{
			if (vsize < min_vsize)
			{
				min_vsize = vsize;
			}
			if (vsize > max_vsize)
			{
				max_vsize = vsize;
			}
		}
		else if (debug_tex_prio)
		{
			LLViewerFetchedTexture* texp =
				LLViewerTextureManager::staticCast(imagep);
			if (texp)
			{
				F32 pri = texp->getDecodePriority();
				pri = llmax(pri, 0.f);
				if (pri < min_vsize)
				{
					min_vsize = pri;
				}
				if (pri > max_vsize)
				{
					max_vsize = pri;
				}
			}
		}
		else if (debug_face_alpha)
		{
			F32 pri = mPixelArea;
			if (pri < min_vsize)
			{
				min_vsize = pri;
			}
			if (pri > max_vsize)
			{
				max_vsize = pri;
			}
		}
	}

	if (isSculpted())
	{
		// Note: sets mSculptTexture to NULL if this is a mesh object:
		updateSculptTexture();

		if (mSculptTexture.notNull())
		{
			mSculptTexture->setForSculpt();

			if (!mSculptTexture->isCachedRawImageReady())
			{
				S32 lod = llmin(mLOD, 3);
				F32 lodf = (F32)(lod + 1) * 0.25f;
				F32 tex_size = lodf * MAX_SCULPT_REZ;
				mSculptTexture->addTextureStats(2.f * tex_size * tex_size, false);

				// If the sculpty very close to the view point, load first
				LLVector3 look_at = getPositionAgent() -
									gViewerCamera.getOrigin();
				F32 dist = look_at.normalize();
				F32 cos_to_view_dir = look_at * gViewerCamera.getXAxis();
				F32 prio = 0.8f *
						   LLFace::calcImportanceToCamera(cos_to_view_dir,
														  dist);
				mSculptTexture->setAdditionalDecodePriority(prio);
			}

			// Try to match the texture:
			S32 texture_discard = mSculptTexture->getCachedRawImageLevel();
			S32 current_discard = getVolume() ? getVolume()->getSculptLevel()
											  : -2;

			if (texture_discard >= 0 && // Texture has some data available
				 // Texture has more data than last rebuild
				(texture_discard < current_discard ||
				 // No previous rebuild
				 current_discard < 0))
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
				mSculptChanged = true;
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SCULPTED))
			{
				setDebugText(llformat("T%d C%d V%d\n%dx%d",
									  texture_discard, current_discard,
									  getVolume() ?
										getVolume()->getSculptLevel() : -2,
									  mSculptTexture->getHeight(),
									  mSculptTexture->getWidth()));
			}
		}
	}

	if (getLightTextureID().notNull())
	{
		const LLLightImageParams* params = getLightImageParams();
		if (params)
		{
			const LLUUID& id = params->getLightTexture();
			mLightTexture =
				LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT,
														  true,
														  LLGLTexture::BOOST_ALM);
			if (mLightTexture.notNull())
			{
				F32 rad = getLightRadius();
				F32 vsize = gPipeline.calcPixelArea(getPositionAgent(),
													LLVector3(rad, rad, rad),
													gViewerCamera);
				mLightTexture->addTextureStats(vsize);
			}
		}
	}

	if (debug_tex_area)
	{
		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
	}
 	else if (debug_tex_prio)
 	{
 		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
 	}
	else if (debug_face_alpha)
	{
		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_SIZE))
	{
		std::map<U64, std::string> tex_list;
		std::map<U64, std::string>::iterator it;
		std::string faces;
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (!facep) continue;

			LLViewerTexture* vtexp = facep->getTexture();
			if (!vtexp) continue;

			LLViewerFetchedTexture* texp = vtexp->asFetched();
			if (!texp) continue;

			faces = llformat("%d", i);
			U64 size = ((U64)texp->getWidth() << 32) + (U64)texp->getHeight();
			it = tex_list.find(size);
			if (it == tex_list.end())
			{
				tex_list.emplace(size, faces);
			}
			else
			{
				tex_list.emplace(size, it->second + " " + faces);
			}
		}

		std::map<U64, std::string>::iterator end = tex_list.end();
		std::string output;
		for (it = tex_list.begin(); it != end; ++it)
		{
			U64 size = it->first;
			S32 width = (S32)(size >> 32);
			S32 height = (S32)(size & 0x00000000ffffffff);
			faces = llformat("%dx%d (%s)", width, height, it->second.c_str());
			if (!output.empty())
			{
				output += "\n";
			}
			output += faces;
		}
		setDebugText(output);
	}

	if (mPixelArea == 0)
	{
		// Flexi phasing issues make this happen
		mPixelArea = old_area;
	}
}

void LLVOVolume::setTexture(S32 face)
{
	llassert(face < getNumTEs());
	gGL.getTexUnit(0)->bind(getTEImage(face));
}

void LLVOVolume::setScale(const LLVector3& scale, bool damped)
{
	if (scale != getScale())
	{
		// Store local radius
		LLViewerObject::setScale(scale);

		if (mVolumeImpl)
		{
			mVolumeImpl->onSetScale(scale, damped);
		}

		updateRadius();

		if (mDrawable.notNull())
		{
			// Since drawable transforms do not include scale, changing volume
			// scale requires a rebuild of volume verts.
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_POSITION);
			shrinkWrap();
		}
	}
}

LLFace* LLVOVolume::addFace(S32 f)
{
	const LLTextureEntry* tep = getTE(f);
	LLViewerTexture* imagep = getTEImage(f);
	if (tep && tep->getMaterialParams().notNull())
	{
		return mDrawable->addFace(tep, imagep, getTENormalMap(f),
								  getTESpecularMap(f));
	}
	return mDrawable->addFace(tep, imagep);
}

LLDrawable* LLVOVolume::createDrawable()
{
	gPipeline.allocDrawable(this);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_VOLUME);

	S32 max_tes_to_set = getNumTEs();
	for (S32 i = 0; i < max_tes_to_set; ++i)
	{
		addFace(i);
	}
	mNumFaces = max_tes_to_set;

	if (isAttachment())
	{
		mDrawable->makeActive();
	}

	if (getIsLight())
	{
		// Add it to the pipeline mLightSet
		gPipeline.setLight(mDrawable, true);
	}

	if (isReflectionProbe())
	{
		updateReflectionProbePtr();
	}

	updateRadius();
	// Force_update = true to avoid non-alpha mDistance update being optimized
	// away
	mDrawable->updateDistance(gViewerCamera, true);

	return mDrawable;
}

bool LLVOVolume::setVolume(const LLVolumeParams& params_in, S32 detail,
						   bool unique_volume)
{
	mCostData = NULL;	//  Reset cost data cache since parameters changed

	LLVolumeParams volume_params = params_in;

	S32 last_lod;
	if (mVolumep.notNull())
	{
		last_lod =
			LLVolumeLODGroup::getVolumeDetailFromScale(mVolumep->getDetail());
	}
	else
	{
		last_lod = -1;
	}

	bool is404 = false;
	S32 lod = mLOD;
	if (isSculpted())
	{
		// If it is a mesh
		if ((volume_params.getSculptType() & LL_SCULPT_TYPE_MASK) ==
				LL_SCULPT_TYPE_MESH)
		{
			lod = gMeshRepo.getActualMeshLOD(volume_params, lod);
			if (lod == -1)
			{
				is404 = true;
				lod = 0;
			}
			else
			{
				mLOD = lod;	// Adopt the actual mesh LOD
			}
		}
	}

	// Check if we need to change implementations
	bool is_flexible = volume_params.getPathParams().getCurveType() ==
						LL_PCODE_PATH_FLEXIBLE;
	if (is_flexible)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, true, false);
		if (!mVolumeImpl)
		{
			mVolumeImpl = new LLVolumeImplFlexible(this,
												   getFlexibleObjectData());
		}
	}
	else
	{
		// Mark the parameter not in use
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, false, false);
		if (mVolumeImpl)
		{
			delete mVolumeImpl;
			mVolumeImpl = NULL;
			if (mDrawable.notNull())
			{
				// Undo the damage we did to this matrix
				mDrawable->updateXform(false);
			}
		}
	}

	if (is404)
	{
		setIcon(LLViewerTextureManager::getFetchedTextureFromFile("inv_item_mesh.tga"));
		// Render prim proxy when mesh loading attempts give up
		volume_params.setSculptID(LLUUID::null, LL_SCULPT_TYPE_NONE);
	}

	bool res = LLPrimitive::setVolume(volume_params, lod,
									  mVolumeImpl &&
									  mVolumeImpl->isVolumeUnique());
	if (!res && !mSculptChanged)
	{
		return false;
	}

	mFaceMappingChanged = true;

	if (mVolumeImpl)
	{
		mVolumeImpl->onSetVolume(volume_params, mLOD);
	}

	updateSculptTexture();

	if (!isSculpted())
	{
		return true;
	}

	LLVolume* volp = getVolume();
	if (!volp)
	{
		return false;
	}

	// If it is a mesh
	if ((volume_params.getSculptType() &
		 LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH)
	{
		if (mSkinInfo.notNull() &&
			mSkinInfo->mMeshID != volume_params.getSculptID())
		{
			mSkinInfo = NULL;
			mSkinInfoFailed = false;
		}
		if (volp && !volp->isMeshAssetLoaded())
		{
			// Load request not yet issued, request pipeline load this mesh
			S32 available_lod = gMeshRepo.loadMesh(this, volume_params, lod,
												   last_lod);
			if (available_lod != lod)
			{
				LLPrimitive::setVolume(volume_params, available_lod);
			}
		}
		if (mSkinInfo.isNull() && !mSkinInfoFailed)
		{
			const LLMeshSkinInfo* skin_infop =
				gMeshRepo.getSkinInfo(volume_params.getSculptID(), this);
			if (skin_infop)
			{
				notifySkinInfoLoaded((LLMeshSkinInfo*)skin_infop);
			}
		}
	}
	// Otherwise it should be sculptie
	else
	{
		sculpt();
	}

	return true;
}

void LLVOVolume::updateSculptTexture()
{
	LLPointer<LLViewerFetchedTexture> old_sculpt = mSculptTexture;

	if (isSculpted() && !isMesh())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			const LLUUID& id = sculpt_params->getSculptTexture();
			if (id.notNull())
			{
				mSculptTexture =
					LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT,
															  true,
															  LLGLTexture::BOOST_NONE,
															  LLViewerTexture::LOD_TEXTURE);
			}
		}
		mSkinInfoFailed = false;
		mSkinInfo = NULL;
	}
	else
	{
		mSculptTexture = NULL;
	}

	if (mSculptTexture != old_sculpt)
	{
		if (old_sculpt.notNull())
		{
			old_sculpt->removeVolume(LLRender::SCULPT_TEX, this);
		}
		if (mSculptTexture.notNull())
		{
			mSculptTexture->addVolume(LLRender::SCULPT_TEX, this);
		}
	}
}

void LLVOVolume::updateVisualComplexity()
{
	LLVOAvatar* avatarp = getAvatarAncestor();
	if (avatarp)
	{
		avatarp->updateVisualComplexity();
	}
	LLVOAvatar* rigged_avatarp = getAvatar();
	if (rigged_avatarp && rigged_avatarp != avatarp)
	{
		rigged_avatarp->updateVisualComplexity();
	}
}

void LLVOVolume::notifyMeshLoaded()
{
	mCostData = NULL;	//  Reset cost data cache since mesh changed
	mSculptChanged = true;
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY);
	LLVOAvatar* avatarp = getAvatar();
	if (avatarp && !isAnimatedObject())
	{
		avatarp->addAttachmentOverridesForObject(this);
	}
	LLVOAvatarPuppet* puppetp = getPuppetAvatar();
	if (puppetp)
	{
		puppetp->addAttachmentOverridesForObject(this);
	}
	updateVisualComplexity();
}

void LLVOVolume::notifySkinInfoLoaded(LLMeshSkinInfo* skinp)
{
	mSkinInfoFailed = false;
	mSkinInfo = skinp;
	notifyMeshLoaded();
}

void LLVOVolume::notifySkinInfoUnavailable()
{
	mSkinInfoFailed = true;
	mSkinInfo = NULL;
}

// This replaces generate() for sculpted surfaces
void LLVOVolume::sculpt()
{
	if (mSculptTexture.isNull())
	{
		return;
	}

	U16 sculpt_height = 0;
	U16 sculpt_width = 0;
	S8 sculpt_components = 0;
	const U8* sculpt_data = NULL;

	S32 discard_level = mSculptTexture->getCachedRawImageLevel();
	LLImageRaw* raw_image = mSculptTexture->getCachedRawImage();

	S32 max_discard = mSculptTexture->getMaxDiscardLevel();
	if (discard_level > max_discard)
	{
		discard_level = max_discard;	// Clamp to the best we can do
	}
	if (discard_level > MAX_DISCARD_LEVEL)
	{
		return; // We think data is not ready yet.
	}

	S32 current_discard = getVolume()->getSculptLevel();
	if (current_discard < -2)
	{
		llwarns << "Current discard of sculpty at " << current_discard
				<< " is less than -2 !" << llendl;
		// Corrupted volume... do not update the sculpty
		return;
	}
	else if (current_discard > MAX_DISCARD_LEVEL)
	{
#if 0	// Disabled to avoid log spam. *FIXME: find the cause for that spam
		llwarns << "Current discard of sculpty at " << current_discard
				<< " is more than the allowed max of " << MAX_DISCARD_LEVEL
				<< llendl;
#endif
		// Corrupted volume... do not update the sculpty
		return;
	}

	if (current_discard == discard_level)
	{
		// No work to do here
		return;
	}

	if (!raw_image)
	{
		llassert(discard_level < 0);
		sculpt_width = 0;
		sculpt_height = 0;
		sculpt_data = NULL;
	}
	else
	{
		sculpt_height = raw_image->getHeight();
		sculpt_width = raw_image->getWidth();
		sculpt_components = raw_image->getComponents();
		sculpt_data = raw_image->getData();
	}
	getVolume()->sculpt(sculpt_width, sculpt_height, sculpt_components,
						sculpt_data, discard_level,
						mSculptTexture->isMissingAsset());

	// Notify rebuild any other VOVolumes that reference this sculpty volume
	for (S32 i = 0,
			 count = mSculptTexture->getNumVolumes(LLRender::SCULPT_TEX);
		 i < count; ++i)
	{
		LLVOVolume* volp =
			(*(mSculptTexture->getVolumeList(LLRender::SCULPT_TEX)))[i];
		if (volp != this && volp->getVolume() == getVolume())
		{
			gPipeline.markRebuild(volp->mDrawable,
								  LLDrawable::REBUILD_GEOMETRY);
		}
	}
}

S32 LLVOVolume::computeLODDetail(F32 distance, F32 radius, F32 lod_factor)
{
	if (LLPipeline::sDynamicLOD)
	{
		// We have got LOD in the profile, and in the twist. Use radius.
		F32 tan_angle = ll_round(lod_factor * radius / distance, 0.01f);
		return LLVolumeLODGroup::getDetailFromTan(tan_angle);
	}
	return llclamp((S32)(sqrtf(radius) * lod_factor * 4.f), 0, 3);
}

bool LLVOVolume::calcLOD()
{
	if (mDrawable.isNull())
	{
		return false;
	}

	// Locked to max LOD objects, selected objects and HUD attachments always
	// rendered at max LOD
	if (mLockMaxLOD || isSelected() || isHUDAttachment())
	{
		if (mLOD == 3)
		{
			return false;
		}
		mLOD = 3;
		return true;
	}

	F32 radius;
	F32 distance;
	LLVolume* volumep = getVolume();
	if (mDrawable->isState(LLDrawable::RIGGED))
	{
		LLVOAvatar* avatarp = getAvatar();
		if (!avatarp)
		{
			llwarns << "NULL avatar pointer for rigged drawable" << llendl;
			clearRiggedVolume();	// Bogus volume: clear it. HB
			return false;
		}
		if (avatarp->mDrawable.isNull())
		{
			llwarns << "No drawable for avatar associated to rigged drawable"
					<< llendl;
			clearRiggedVolume();	// Bogus volume: clear it. HB
			return false;
		}
		distance = avatarp->mDrawable->mDistanceWRTCamera;
		if (avatarp->isPuppetAvatar())
		{
			// Handle volumes in an animated object as a special case
			const LLVector3* boxp = avatarp->getLastAnimExtents();
			radius = (boxp[1] - boxp[0]).length() * 0.5f;
		}
		else
		{
#if 1		// SL-937: add dynamic box handling for rigged mesh on regular
			// avatars
			const LLVector3* boxp = avatarp->getLastAnimExtents();
			radius = (boxp[1] - boxp[0]).length();
#else
			radius = avatarp->getBinRadius();
#endif
		}
	}
	else
	{
		if (volumep)
		{
			radius = volumep->mLODScaleBias.scaledVec(getScale()).length();
		}
		else
		{
			llwarns_once << "NULL volume associated with drawable " << std::hex
						 << mDrawable.get() << std::dec << llendl;
			radius = getScale().length();
		}
		distance = mDrawable->mDistanceWRTCamera;
	}

	if (distance <= 0.f || radius <= 0.f)
	{
		return false;
	}

	radius = ll_round(radius, 0.01f);
	distance *= sDistanceFactor;

	static LLCachedControl<F32> mesh_boost(gSavedSettings,
										   "MeshLODBoostFactor");
	F32 boost_factor = 1.f;
	if (mesh_boost > 1.f && isMesh())
	{
		boost_factor = llclamp((F32)mesh_boost, 1.f, 4.f);
	}

	// Boost LOD when you are REALLY close
	F32 ramp_dist = sLODFactor * 2.f * boost_factor;
	if (distance < ramp_dist)
	{
		distance /= ramp_dist;
		distance *= distance;
		distance *= ramp_dist;
	}
	distance = ll_round(distance * (F_PI / 3.f), 0.01f);

	static LLCachedControl<F32> hysteresis(gSavedSettings,
										   "DistanceHysteresisLOD");
	// Avoid blinking objects due to LOD changing every few frames because of
	// LOD-dependant (since bounding-box dependant) distance changes. HB
	if (mLastDistance > 0.f && fabs(mLastDistance - distance) < hysteresis)
	{
		distance = mLastDistance;
	}
	else
	{
		mLastDistance = distance;
	}

	F32 lod_factor = sLODFactor;
	static LLCachedControl<bool> ignore_fov_zoom(gSavedSettings,
												 "IgnoreFOVZoomForLODs");
	if (!ignore_fov_zoom)
	{
		lod_factor *= DEFAULT_FIELD_OF_VIEW / gViewerCamera.getDefaultFOV();
	}
	lod_factor *= boost_factor;

	S32 cur_detail = computeLODDetail(distance, radius, lod_factor);

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_LOD_INFO))
	{
		setDebugText(llformat("%d (d=%.2f/r=%.2f)", cur_detail, distance,
							  radius));
	}

	if (cur_detail == mLOD)
	{
		return false;
	}

	mAppAngle = ll_round(atan2f(mDrawable->getRadius(),
								mDrawable->mDistanceWRTCamera) * RAD_TO_DEG,
						 0.01f);
	mLOD = cur_detail;
	return true;
}

bool LLVOVolume::updateLOD()
{
	if (mDrawable.isNull())
	{
		return false;
	}

	if (calcLOD())
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
		mLODChanged = true;
		return true;
	}

	F32 new_radius = getBinRadius();
	F32 old_radius = mDrawable->getBinRadius();
	if (new_radius < old_radius * 0.9f || new_radius > old_radius * 1.1f)
	{
		gPipeline.markPartitionMove(mDrawable);
	}

	return LLViewerObject::updateLOD();
}

void LLVOVolume::tempSetLOD(S32 lod)
{
	mLOD = lod;
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	mLODChanged = true;
}

bool LLVOVolume::setDrawableParent(LLDrawable* parentp)
{
	if (!LLViewerObject::setDrawableParent(parentp))
	{
		// No change in drawable parent
		return false;
	}

	if (!mDrawable->isRoot())
	{
		// Rebuild vertices in parent relative space
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);

		if (mDrawable->isActive() && !parentp->isActive())
		{
			parentp->makeActive();
		}
		else if (mDrawable->isStatic() && parentp->isActive())
		{
			mDrawable->makeActive();
		}
	}

	return true;
}

void LLVOVolume::updateFaceFlags()
{
	if (mDrawable.isNull())
	{
		llwarns << "NULL drawable !" << llendl;
		return;
	}
	for (S32 i = 0, count = llmin(getVolume()->getNumFaces(),
								  mDrawable->getNumFaces());
		 i < count; ++i)
	{
		LLFace* facep = mDrawable->getFace(i);
		if (facep)
		{
			LLTextureEntry* tep = getTE(i);
			if (!tep) continue;

			bool fullbright = tep->getFullbright();
			facep->clearState(LLFace::FULLBRIGHT | LLFace::HUD_RENDER |
							  LLFace::LIGHT);

			if (fullbright || mMaterial == LL_MCODE_LIGHT)
			{
				facep->setState(LLFace::FULLBRIGHT);
			}
			if (mDrawable->isLight())
			{
				facep->setState(LLFace::LIGHT);
			}
			if (isHUDAttachment())
			{
				facep->setState(LLFace::HUD_RENDER);
			}
		}
	}
}

bool LLVOVolume::setParent(LLViewerObject* parentp)
{
	bool ret = false;

	LLViewerObject* old_parentp = (LLViewerObject*)getParent();
	if (parentp != old_parentp)
	{
		ret = LLViewerObject::setParent(parentp);
		if (ret && mDrawable)
		{
			gPipeline.markMoved(mDrawable);
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
		}
		onReparent(old_parentp, parentp);
	}

	return ret;
}

void LLVOVolume::regenFaces()
{
	// Remove existing faces
	bool count_changed = mNumFaces != getNumTEs();
	if (count_changed)
	{
		deleteFaces();
		// Add new faces
		mNumFaces = getNumTEs();
	}

	S32 media_count = mMediaImplList.size();
	for (S32 i = 0; i < mNumFaces; ++i)
	{
		LLFace* facep = count_changed ? addFace(i) : mDrawable->getFace(i);
		if (!facep) continue;

		facep->setTEOffset(i);
		facep->setDiffuseMap(getTEImage(i));
		if (facep->getTextureEntry()->getMaterialParams().notNull())
		{
			facep->setNormalMap(getTENormalMap(i));
			facep->setSpecularMap(getTESpecularMap(i));
		}
		facep->setViewerObject(this);

		if (i >= media_count || mMediaImplList[i].isNull())
		{
			continue;
		}

		// If the face had media on it, this will have broken the link between
		// the LLViewerMediaTexture and the face. Re-establish the link.
		const LLUUID& id = mMediaImplList[i]->getMediaTextureID();
		LLViewerMediaTexture* media_texp =
			LLViewerTextureManager::findMediaTexture(id);
		if (media_texp)
		{
			media_texp->addMediaToFace(facep);
		}
	}

	if (!count_changed)
	{
		updateFaceFlags();
	}
}

bool LLVOVolume::genBBoxes(bool force_global, bool update_bounds)
{
	bool rebuild = mDrawable->isState(LLDrawable::REBUILD_VOLUME |
									  LLDrawable::REBUILD_POSITION |
									  LLDrawable::REBUILD_RIGGED);

	LLVolume* volumep = mRiggedVolume.get();
	if (volumep)
	{
		// With 'false', this will remove unused rigged volumes, which we are
		// not currently very aggressive about.
		updateRiggedVolume(false);
	}
	else
	{
		volumep = getVolume();
		if (!volumep)
		{
			llwarns_sparse << "NULL volume. Skipping." << llendl;
			return false;
		}
	}

	bool res = true;

	LLVector4a min, max;
	min.clear();
	max.clear();

	force_global |= mVolumeImpl && mVolumeImpl->isVolumeGlobal();
	bool any_valid_boxes = false;
	for (S32 i = 0, count = llmin(volumep->getNumVolumeFaces(),
								  mDrawable->getNumFaces(),
								  (S32)getNumTEs()); i < count; ++i)
	{
		LLFace* facep = mDrawable->getFace(i);
		if (!facep)
		{
			continue;
		}
		bool face_res = facep->genVolumeBBoxes(*volumep, i, mRelativeXform,
											   force_global);
		res &= face_res;
		if (!face_res)
		{
			// MAINT-8264: ignore bboxes of ill-formed faces.
			continue;
		}
		if (rebuild)
		{
			if (!any_valid_boxes)
			{
				min = facep->mExtents[0];
				max = facep->mExtents[1];
				any_valid_boxes = true;
			}
			else
			{
				min.setMin(min, facep->mExtents[0]);
				max.setMax(max, facep->mExtents[1]);
			}
		}
	}

	if (any_valid_boxes)
	{
		if (rebuild && update_bounds)
		{
			mDrawable->setSpatialExtents(min, max);
			bool has_avatar = false;
			if (isRiggedMesh())
			{
				// When editing any attachment, skip entirely the 'has_avatar'
				// optimization for rigged mesh octree/batching, and revert to
				// the old code. The reason for this is that the new optimized
				// code breaks our work-around (see the "EditedMeshLOD" debug
				// setting usage) for broken rigged mesh LODs while edited (and
				// the mesh LOD sometimes even stays broken after edit without
				// that work-around). HB
				if (!LLFloaterTools::isVisible() ||
					!gSelectMgr.selectionIsAvatarAttachment())
				{
					if (isAnimatedObject())
					{
						LLVOAvatarPuppet* puppetp = getPuppetAvatar();
						has_avatar = puppetp && puppetp->mPlaying;
					}
					else
					{
						has_avatar = isAttachment() && getAvatar();
					}
				}
			}
			if (has_avatar)
			{
				// Put all rigged drawables in the same octree node for
				// better batching.
				mDrawable->setPositionGroup(LLVector4a::getZero());
			}
			else
			{
				min.add(max);
				min.mul(0.5f);
				mDrawable->setPositionGroup(min);
			}
		}

		updateRadius();
		mDrawable->movePartition();
	}

	return res;
}

void LLVOVolume::preRebuild()
{
	if (mVolumeImpl)
	{
		mVolumeImpl->preRebuild();
	}
}

void LLVOVolume::updateRelativeXform(bool force_identity)
{
	if (mVolumeImpl)
	{
		mVolumeImpl->updateRelativeXform(force_identity);
		return;
	}

	static LLVector3 vec3_x(1.f, 0.f, 0.f);
	static LLVector3 vec3_y(0.f, 1.f, 0.f);
	static LLVector3 vec3_z(0.f, 0.f, 1.f);

	LLDrawable* drawable = mDrawable;

	if (drawable->isState(LLDrawable::RIGGED) && mRiggedVolume.notNull())
	{
		// Rigged volume (which is in agent space) is used for generating
		// bounding boxes etc. Inverse of render matrix should go to
		// partition space
		mRelativeXform = getRenderMatrix();

		F32* dst = mRelativeXformInvTrans.getF32ptr();
		F32* src = mRelativeXform.getF32ptr();
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[4];
		dst[4] = src[5];
		dst[5] = src[6];
		dst[6] = src[8];
		dst[7] = src[9];
		dst[8] = src[10];

		mRelativeXform.invert();
		mRelativeXformInvTrans.transpose();
	}
	else if (drawable->isActive() || force_identity)
	{
		// Setup relative transforms
		LLQuaternion delta_rot;
		LLVector3 delta_pos;
		// Matrix from local space to parent relative/global space
		if (!force_identity && !drawable->isSpatialRoot())
		{
			delta_rot = mDrawable->getRotation();
			delta_pos = mDrawable->getPosition();
		}
		LLVector3 delta_scale = mDrawable->getScale();

		// Vertex transform (4x4)
		LLVector3 x_axis = LLVector3(delta_scale.mV[VX], 0.f, 0.f) * delta_rot;
		LLVector3 y_axis = LLVector3(0.f, delta_scale.mV[VY], 0.f) * delta_rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, delta_scale.mV[VZ]) * delta_rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(delta_pos, 1.f));

		// Compute inverse transpose for normals
		// grumble - invert is NOT a matrix invert, so we do it by hand:

		LLMatrix3 rot_inverse = LLMatrix3(~delta_rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(vec3_x / delta_scale.mV[VX],
							  vec3_y / delta_scale.mV[VY],
							  vec3_z / delta_scale.mV[VZ]);

		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}
	else
	{
		LLVector3 pos = getPosition();
		LLVector3 scale = getScale();
		LLQuaternion rot = getRotation();

		if (mParent)
		{
			pos *= mParent->getRotation();
			pos += mParent->getPosition();
			rot *= mParent->getRotation();
		}
#if 0
		pos += getRegion()->getOriginAgent();
#endif

		LLVector3 x_axis = LLVector3(scale.mV[VX], 0.f, 0.f) * rot;
		LLVector3 y_axis = LLVector3(0.f, scale.mV[VY], 0.f) * rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, scale.mV[VZ]) * rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(pos, 1.f));

		// Compute inverse transpose for normals
		LLMatrix3 rot_inverse = LLMatrix3(~rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(vec3_x / scale.mV[VX],
							  vec3_y / scale.mV[VY],
							  vec3_z / scale.mV[VZ]);

		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}
}

bool LLVOVolume::lodOrSculptChanged(LLDrawable* drawable, bool& update_bounds)
{
	LLVolume* old_volumep = getVolume();
	if (!old_volumep)
	{
		return false;
	}
	F32 old_lod = old_volumep->getDetail();
	S32 old_num_faces = old_volumep->getNumFaces();;
	old_volumep = NULL;

	LLVolume* new_volumep = getVolume();
	{
		LL_FAST_TIMER(FTM_GEN_VOLUME);
		const LLVolumeParams& volume_params = new_volumep->getParams();
		setVolume(volume_params, 0);
	}
	F32 new_lod = new_volumep->getDetail();
	S32 new_num_faces = new_volumep->getNumFaces();
	new_volumep = NULL;

	bool regen_faces = false;
	if (new_lod != old_lod || mSculptChanged)
	{
		if (mDrawable->isState(LLDrawable::RIGGED))
		{
			updateVisualComplexity();
		}

		sNumLODChanges += new_num_faces;

		if (new_lod > old_lod || mSculptChanged)
		{
			update_bounds = true;
		}

		if ((S32)getNumTEs() != getVolume()->getNumFaces())
		{
			// Mesh loading may change number of faces.
			setNumTEs(getVolume()->getNumFaces());
		}

		// For face->genVolumeTriangles()
		drawable->setState(LLDrawable::REBUILD_VOLUME);

		{
			LL_FAST_TIMER(FTM_GEN_TRIANGLES);
			regen_faces = new_num_faces != old_num_faces ||
						  mNumFaces != (S32)getNumTEs();
			if (regen_faces)
			{
				regenFaces();
			}

			if (mSculptChanged)
			{
				// Changes in sculpt maps can thrash an object bounding box
				// without triggering a spatial group bounding box update:
				// force spatial group to update bounding boxes.
				LLSpatialGroup* group = mDrawable->getSpatialGroup();
				if (group)
				{
					group->unbound();
				}
			}
		}
	}

	return regen_faces;
}

bool LLVOVolume::updateGeometry(LLDrawable* drawablep)
{
	LL_FAST_TIMER(FTM_UPDATE_PRIMITIVES);

	if (isDead() || !drawablep || drawablep->isDead() || mDrawable.isNull() ||
		mDrawable->isDead())
	{
		return true;
	}

	if (mDrawable->isState(LLDrawable::REBUILD_RIGGED))
	{
		updateRiggedVolume(false);
		genBBoxes(false);
		mDrawable->clearState(LLDrawable::REBUILD_RIGGED);
	}

	if (mVolumeImpl)
	{
		bool res;
		{
			LL_FAST_TIMER(FTM_GEN_FLEX);
			res = mVolumeImpl->doUpdateGeometry(drawablep);
		}
		updateFaceFlags();
		return res;
	}

	LLSpatialGroup* groupp = drawablep->getSpatialGroup();
	if (groupp)
	{
		groupp->dirtyMesh();
	}

	updateRelativeXform();

	// Not sure why this is happening, but it is...
	if (mDrawable.isNull() || mDrawable->isDead())
	{
		llwarns << "NULL or dead drawable detected. Aborted." << llendl;
		return true; // No update to complete
	}

	// This should be true in most cases, unless we are sure no octree update
	// is needed.
	bool update_bounds = mRiggedVolume.notNull() ||
						 mDrawable->isState(LLDrawable::REBUILD_POSITION) ||
						 !mDrawable->getSpatialExtents()->isFinite3();

	if (mVolumeChanged || mFaceMappingChanged)
	{
		dirtySpatialGroup();

		bool was_regen_faces = false;
		update_bounds = true;
		if (mVolumeChanged)
		{
			was_regen_faces = lodOrSculptChanged(drawablep, update_bounds);
			drawablep->setState(LLDrawable::REBUILD_VOLUME);
		}
		else if (mSculptChanged || mLODChanged || mColorChanged)
		{
			was_regen_faces = lodOrSculptChanged(drawablep, update_bounds);
		}

		if (!was_regen_faces)
		{
			LL_FAST_TIMER(FTM_GEN_TRIANGLES);
			regenFaces();
		}
	}
	else if (mLODChanged || mSculptChanged || mColorChanged)
	{
		dirtySpatialGroup();
		lodOrSculptChanged(drawablep, update_bounds);
		constexpr U32 rigged = LLDrawable::REBUILD_RIGGED | LLDrawable::RIGGED;
		if (drawablep->isState(rigged))
		{
			updateRiggedVolume(false);
		}
	}

	// Generate bounding boxes if needed, and update the object size in the
	// octree
	genBBoxes(false, update_bounds);

	// Update face flags
	updateFaceFlags();

	mVolumeChanged = mLODChanged = mSculptChanged = mColorChanged =
					 mFaceMappingChanged = false;

	return LLViewerObject::updateGeometry(drawablep);
}

//virtual
void LLVOVolume::updateFaceSize(S32 idx)
{
	if (mDrawable->getNumFaces() <= idx)
	{
		return;
	}

	LLFace* facep = mDrawable->getFace(idx);
	if (!facep)
	{
		return;
	}

	if (idx >= getVolume()->getNumVolumeFaces())
	{
		facep->setSize(0, 0, true);
	}
	else
	{
		const LLVolumeFace& vol_face = getVolume()->getVolumeFace(idx);
		facep->setSize(vol_face.mNumVertices, vol_face.mNumIndices,
					   // volume faces must be padded for 16-byte alignment
					   true);
	}
}

//virtual
void LLVOVolume::setNumTEs(U8 num_tes)
{
	U8 old_num_tes = getNumTEs();

	if (old_num_tes && old_num_tes < num_tes) // new faces added
	{
		LLViewerObject::setNumTEs(num_tes);

		// Duplicate the last media textures if exists.
		if (mMediaImplList.size() >= old_num_tes &&
			mMediaImplList[old_num_tes - 1].notNull())
		{
			mMediaImplList.resize(num_tes);
			const LLTextureEntry* tep = getTE(old_num_tes - 1);
			for (U8 i = old_num_tes; i < num_tes; ++i)
			{
				setTE(i, *tep);
				mMediaImplList[i] = mMediaImplList[old_num_tes - 1];
			}
			mMediaImplList[old_num_tes - 1]->setUpdated(true);
		}
		return;
	}

	if (old_num_tes > num_tes && mMediaImplList.size() > num_tes)
	{
		// Old faces removed
		for (U8 i = num_tes, end = mMediaImplList.size(); i < end; ++i)
		{
			removeMediaImpl(i);
		}
		mMediaImplList.resize(num_tes);
	}

	LLViewerObject::setNumTEs(num_tes);
}

//virtual
void LLVOVolume::setTEImage(U8 te, LLViewerTexture* imagep)
{
	bool changed = getTEImage(te) != imagep;
	LLViewerObject::setTEImage(te, imagep);
	if (changed)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
}

//virtual
S32 LLVOVolume::setTETexture(U8 te, const LLUUID& uuid)
{
	S32 res = LLViewerObject::setTETexture(te, uuid);
	if (res)
	{
		if (mDrawable.notNull())
		{
			gPipeline.markTextured(mDrawable);
			// Dynamic texture changes break batches: isolate in octree.
			shrinkWrap();
		}
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEColor(U8 te, const LLColor3& color)
{
	return setTEColor(te, LLColor4(color));
}

//virtual
S32 LLVOVolume::setTEColor(U8 te, const LLColor4& color)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
		return 0;
	}

	if (color == tep->getColor())
	{
		return 0;
	}

	if (color.mV[3] != tep->getAlpha())
	{
		gPipeline.markTextured(mDrawable);
		// Treat this alpha change as an LoD update since render batches may
		// need to get rebuilt
		mLODChanged = true;
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	}

	S32 retval = LLPrimitive::setTEColor(te, color);
	if (retval && mDrawable.notNull())
	{
		// These should only happen on updates which are not the initial
		// update.
		mColorChanged = true;
		mDrawable->setState(LLDrawable::REBUILD_COLOR);
		shrinkWrap();
		dirtyMesh();
	}
	return  retval;
}

//virtual
S32 LLVOVolume::setTEBumpmap(U8 te, U8 bumpmap)
{
	S32 res = LLViewerObject::setTEBumpmap(te, bumpmap);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTETexGen(U8 te, U8 texgen)
{
	S32 res = LLViewerObject::setTETexGen(te, texgen);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEMediaTexGen(U8 te, U8 media)
{
	S32 res = LLViewerObject::setTEMediaTexGen(te, media);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEShiny(U8 te, U8 shiny)
{
	S32 res = LLViewerObject::setTEShiny(te, shiny);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEFullbright(U8 te, U8 fullbright)
{
	S32 res = LLViewerObject::setTEFullbright(te, fullbright);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEBumpShinyFullbright(U8 te, U8 bump)
{
	S32 res = LLViewerObject::setTEBumpShinyFullbright(te, bump);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEMediaFlags(U8 te, U8 media_flags)
{
	S32 res = LLViewerObject::setTEMediaFlags(te, media_flags);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEGlow(U8 te, F32 glow)
{
	S32 res = LLViewerObject::setTEGlow(te, glow);
	if (res)
	{
		if (mDrawable.notNull())
		{
			gPipeline.markTextured(mDrawable);
			shrinkWrap();
		}
		mFaceMappingChanged = true;
	}
	return res;
}

//static
void LLVOVolume::setTEMaterialParamsCallbackTE(const LLUUID& objid,
											   const LLMaterialID& matidp,
											   const LLMaterialPtr paramsp,
											   U32 te)
{
	LLVOVolume* volp = (LLVOVolume*)gObjectList.findObject(objid);
	if (!volp) return;	// stale callback for removed object

	if (te >= volp->getNumTEs())
	{
		llwarns << "Got a callback for materialid " << matidp.asString()
				<< " with an out of range face number: " << te << ". Ignoring."
				<< llendl;
		return;
	}

	LLTextureEntry* tep = volp->getTE(te);
	if (tep && tep->getMaterialID() == matidp)
	{
		LL_DEBUGS("Materials") << "Applying materialid " << matidp.asString()
							   << " to face " << te << LL_ENDL;
		volp->setTEMaterialParams(te, paramsp);
	}
}

//virtual
S32 LLVOVolume::setTEMaterialID(U8 te, const LLMaterialID& matidp)
{
	S32 res = LLViewerObject::setTEMaterialID(te, matidp);

	LL_DEBUGS("Materials") << "te = "<< (S32)te << " - materialid = "
						   << matidp.asString() << " - result: " << res;
	bool sel = gSelectMgr.getSelection()->contains(this, te);
	LL_CONT << (sel ? "," : ", not") << " selected" << LL_ENDL;

	if (res == TEM_CHANGE_NONE)
	{
		return res;
	}

	LLViewerRegion* regionp = getRegion();
	if (!regionp)
	{
		return res;
	}

	LLMaterialMgr* matmgrp = LLMaterialMgr::getInstance();
	matmgrp->getTE(regionp->getRegionID(), matidp, te,
				   boost::bind(&LLVOVolume::setTEMaterialParamsCallbackTE,
							   getID(), _1, _2, _3));
	setChanged(ALL_CHANGED);
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
		gPipeline.markRebuild(mDrawable);
	}
	mFaceMappingChanged = true;

	return res;
}

#if LL_FIX_MAT_TRANSPARENCY
// Here, we have confirmation about texture creation, check our wait-list and
// make changes, or return false
bool LLVOVolume::notifyAboutCreatingTexture(LLViewerTexture* texp)
{
	std::pair<mmap_uuid_map_t::iterator, mmap_uuid_map_t::iterator> range =
		mWaitingTextureInfo.equal_range(texp->getID());

	typedef fast_hmap<U8, LLMaterialPtr> map_te_material_t;
	map_te_material_t new_material;

	for (mmap_uuid_map_t::iterator it = range.first;
		 it != range.second; ++it)
	{
		LLMaterialPtr cur_matp = getTEMaterialParams(it->second.te);
		if (cur_matp.isNull()) continue;

		// Here we have interest in DIFFUSE_MAP only !
		if (it->second.map == LLRender::DIFFUSE_MAP &&
			texp->getPrimaryFormat() != GL_RGBA)
		{
			// Let's check the diffuse mode
			U8 mode = cur_matp->getDiffuseAlphaMode();
			if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND ||
				mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE ||
				mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
			{
				// We have non 32 bits texture with DIFFUSE_ALPHA_MODE_* so set
				// mode to DIFFUSE_ALPHA_MODE_NONE instead
				LLMaterialPtr matp = NULL;
				map_te_material_t::iterator it2 =
					new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					matp = new LLMaterial(cur_matp->asLLSD());
					new_material.emplace(it->second.te, matp);
				}
				else
				{
					matp = it2->second;
				}
				matp->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
			}
		}
	}

	if (new_material.empty() || !getRegion())
	{
		// Clear the wait-list
		mWaitingTextureInfo.erase(range.first, range.second);
		return false;
	}

	// Setup new materials
	const LLUUID& regiond_id = getRegion()->getRegionID();
	LLMaterialMgr* matmgrp = LLMaterialMgr::getInstance();
	for (map_te_material_t::const_iterator it = new_material.begin(),
										   end = new_material.end();
		 it != end; ++it)
	{
		matmgrp->setLocalMaterial(regiond_id, it->second);
		LLViewerObject::setTEMaterialParams(it->first, it->second);
	}

	// Clear the wait-list
	mWaitingTextureInfo.erase(range.first, range.second);

	return true;
}

// Here, if we wait information about the texture and it is missing, then
// depending on the texture map (diffuse, normal, or specular), we make changes
// in material and confirm it. If not return false.
bool LLVOVolume::notifyAboutMissingAsset(LLViewerTexture* texp)
{
	std::pair<mmap_uuid_map_t::iterator, mmap_uuid_map_t::iterator> range =
		mWaitingTextureInfo.equal_range(texp->getID());
	if (range.first == range.second)
	{
		return false;
	}

	typedef fast_hmap<U8, LLMaterialPtr> map_te_material_t;
	map_te_material_t new_material;

	for (mmap_uuid_map_t::iterator it = range.first;
		 it != range.second; ++it)
	{
		LLMaterialPtr cur_matp = getTEMaterialParams(it->second.te);
		if (cur_matp.isNull()) continue;

		switch (it->second.map)
		{
			case LLRender::DIFFUSE_MAP:
			{
				if (cur_matp->getDiffuseAlphaMode() !=
						LLMaterial::DIFFUSE_ALPHA_MODE_NONE)
				{
					// Switch to DIFFUSE_ALPHA_MODE_NONE
					LLMaterialPtr matp;
					map_te_material_t::iterator it2 =
						new_material.find(it->second.te);
					if (it2 == new_material.end())
					{
						matp = new LLMaterial(cur_matp->asLLSD());
						new_material.emplace(it->second.te, matp);
					}
					else
					{
						matp = it2->second;
					}
					matp->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
				}
				break;
			}

			case LLRender::NORMAL_MAP:
			{
				// Reset material texture id
				LLMaterialPtr matp;
				map_te_material_t::iterator it2 =
					 new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					matp = new LLMaterial(cur_matp->asLLSD());
					new_material.emplace(it->second.te, matp);
				}
				else
				{
					matp = it2->second;
				}
				matp->setNormalID(LLUUID::null);
				break;
			}

			case LLRender::SPECULAR_MAP:
			{
				// Reset material texture id
				LLMaterialPtr matp;
				map_te_material_t::iterator it2 =
					new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					matp = new LLMaterial(cur_matp->asLLSD());
					new_material.emplace(it->second.te, matp);
				}
				else
				{
					matp = it2->second;
				}
				matp->setSpecularID(LLUUID::null);
			}

			default:
				break;
		}
	}

	if (new_material.empty() || !getRegion())
	{
		// Clear the wait-list
		mWaitingTextureInfo.erase(range.first, range.second);
		return false;
	}

	// Setup new materials
	const LLUUID& regiond_id = getRegion()->getRegionID();
	LLMaterialMgr* matmgrp = LLMaterialMgr::getInstance();
	for (map_te_material_t::const_iterator it = new_material.begin(),
										   end = new_material.end();
		 it != end; ++it)
	{
		matmgrp->setLocalMaterial(regiond_id, it->second);
		LLViewerObject::setTEMaterialParams(it->first, it->second);
	}

	// Clear the wait-list
	mWaitingTextureInfo.erase(range.first, range.second);

	return true;
}
#endif

//virtual
S32 LLVOVolume::setTEMaterialParams(U8 te, const LLMaterialPtr paramsp)
{
#if LL_FIX_MAT_TRANSPARENCY
	LLMaterialPtr matp = const_cast<LLMaterialPtr&>(paramsp);
	if (paramsp)
	{
		LLMaterialPtr new_matp = NULL;
		LLViewerTexture* img_diffuse = getTEImage(te);
		if (img_diffuse)
		{
			if (img_diffuse->getPrimaryFormat() == 0 &&
				!img_diffuse->isMissingAsset())
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(img_diffuse->getID(),
											material_info(LLRender::DIFFUSE_MAP,
														  te));
			}
			else
			{
				bool set_diffuse_none = img_diffuse->isMissingAsset();
				if (!set_diffuse_none)
				{
					U8 mode = paramsp->getDiffuseAlphaMode();
					if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND ||
						mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK ||
						mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE)
					{
						LLTextureEntry* tep = getTE(te);
						bool baked = tep &&
									 LLAvatarAppearanceDictionary::isBakedImageId(tep->getID());
						if (!baked &&
							img_diffuse->getPrimaryFormat() != GL_RGBA)
						{
							set_diffuse_none = true;
						}
					}
				}
				if (set_diffuse_none)
				{
					// Substitute this material with DIFFUSE_ALPHA_MODE_NONE
					new_matp = new LLMaterial(paramsp->asLLSD());
					new_matp->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
				}
			}
		}
		else
		{
			llwarns_sparse << "Missing diffuse channel for material !" << llendl;
			llassert(false);
		}

		const LLUUID& normal_id = paramsp->getNormalID();
		if (normal_id.notNull())
		{
			LLViewerTexture* img_normal = getTENormalMap(te);
			if (img_normal && img_normal->isMissingAsset() &&
				img_normal->getID() == normal_id)
			{
				if (!new_matp)
				{
					new_matp = new LLMaterial(paramsp->asLLSD());
				}
				new_matp->setNormalID(LLUUID::null);
			}
			else if (!img_normal || img_normal->getPrimaryFormat() == 0)
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(normal_id,
											material_info(LLRender::NORMAL_MAP,
														  te));
			}
		}

		const LLUUID& specular_id = paramsp->getSpecularID();
		if (specular_id.notNull())
		{
			LLViewerTexture* img_specular = getTESpecularMap(te);
			if (img_specular && img_specular->isMissingAsset() &&
				img_specular->getID() == specular_id)
			{
				if (!new_matp)
				{
					new_matp = new LLMaterial(paramsp->asLLSD());
				}
				new_matp->setSpecularID(LLUUID::null);
			}
			else if (!img_specular || img_specular->getPrimaryFormat() == 0)
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(specular_id,
											material_info(LLRender::SPECULAR_MAP,
														  te));
			}
		}

		if (new_matp && getRegion())
		{
			matp = new_matp;
			const LLUUID& regiond_id = getRegion()->getRegionID();
			LLMaterialMgr::getInstance()->setLocalMaterial(regiond_id, matp);
		}
	}
	S32 res = LLViewerObject::setTEMaterialParams(te, matp);
	LL_DEBUGS("Materials") << "te = "<< (S32)te << ", material = "
						   << (matp ? matp->asLLSD() : LLSD("null"))
						   << ", res = " << res;
#else
	S32 res = LLViewerObject::setTEMaterialParams(te, paramsp);
	LL_DEBUGS("Materials") << "te = "<< (S32)te << ", material = "
						   << (paramsp ? paramsp->asLLSD() : LLSD("null"))
						   << ", res = " << res;
#endif
	bool sel = gSelectMgr.getSelection()->contains(this, te);
	LL_CONT << (sel ? "," : ", not") << " selected" << LL_ENDL;

	setChanged(ALL_CHANGED);
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
		gPipeline.markRebuild(mDrawable);
	}
	mFaceMappingChanged = true;

	return TEM_CHANGE_TEXTURE;
}

//virtual
S32 LLVOVolume::setTEGLTFMaterialOverride(U8 te, LLGLTFMaterial* matp)
{
	S32 retval = LLViewerObject::setTEGLTFMaterialOverride(te, matp);
	if (retval == TEM_CHANGE_TEXTURE)
	{
		if (mDrawable.notNull())
		{
			gPipeline.markTextured(mDrawable);
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
		}
		mFaceMappingChanged = true;
	}
	return retval;
}

//virtual
S32 LLVOVolume::setTEScale(U8 te, F32 s, F32 t)
{
	S32 res = LLViewerObject::setTEScale(te, s, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEScaleS(U8 te, F32 s)
{
	S32 res = LLViewerObject::setTEScaleS(te, s);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEScaleT(U8 te, F32 t)
{
	S32 res = LLViewerObject::setTEScaleT(te, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

bool LLVOVolume::hasMedia() const
{
	for (U8 i = 0, count = getNumTEs(); i < count; ++i)
	{
		const LLTextureEntry* tep = getTE(i);
		if (tep && tep->hasMedia())
		{
			return true;
		}
	}
	return false;
}

LLVector3 LLVOVolume::getApproximateFaceNormal(U8 face_id)
{
	LLVolume* volp = getVolume();
	if (volp && face_id < volp->getNumVolumeFaces())
	{
		LLVector4a result;
		// Necessary since LLVector4a members are not initialized on creation
		result.clear();

		const LLVolumeFace& face = volp->getVolumeFace(face_id);
		for (S32 i = 0, count = face.mNumVertices; i < count; ++i)
		{
			result.add(face.mNormals[i]);
		}

		LLVector3 ret(result.getF32ptr());
		ret = volumeDirectionToAgent(ret);
		ret.normalize();
		return ret;
	}

	return LLVector3();
}

void LLVOVolume::requestMediaDataUpdate(bool isNew)
{
	if (sObjectMediaClient)
	{
		sObjectMediaClient->fetchMedia(new LLMediaDataClientObjectImpl(this,
																	   isNew));
	}
}

bool LLVOVolume::isMediaDataBeingFetched() const
{
	if (!sObjectMediaClient)
	{
		return false;
	}
	// It is OK to const_cast this away since this is just a wrapper class that
	// is only going to do a lookup.
	LLVOVolume* self = const_cast<LLVOVolume*>(this);
	return sObjectMediaClient->isInQueue(new LLMediaDataClientObjectImpl(self,
																		 false));
}

void LLVOVolume::cleanUpMediaImpls()
{
	// Iterate through our TEs and remove any Impls that are no longer used
	for (U8 i = 0, count = getNumTEs(); i < count; ++i)
	{
		const LLTextureEntry* tep = getTE(i);
		if (tep && !tep->hasMedia())
		{
			// Delete the media implement !
			removeMediaImpl(i);
		}
	}
}

// media_data_array is an array of media entry maps, media_version is the
// version string in the response.
void LLVOVolume::updateObjectMediaData(const LLSD& media_data_array,
									   const std::string& media_version)
{
	U32 fetched_version =
		LLTextureEntry::getVersionFromMediaVersionString(media_version);

	// Only update it if it is newer !
	if ((S32)fetched_version <= mLastFetchedMediaVersion)
	{
		return;
	}

	mLastFetchedMediaVersion = fetched_version;

	LLSD::array_const_iterator iter = media_data_array.beginArray();
	LLSD::array_const_iterator end = media_data_array.endArray();
	U8 tex_idx = 0;
	for ( ; iter != end; ++iter, ++tex_idx)
	{
		syncMediaData(tex_idx, *iter, false, false);
	}
}

void LLVOVolume::syncMediaData(S32 tex_idx, const LLSD& media_data,
							   bool merge, bool ignore_agent)
{
	if (mDead)
	{
		// If the object has been marked dead, do not process media updates.
		return;
	}

	LLTextureEntry* tep = getTE(tex_idx);
	if (!tep)
	{
		return;
	}

	LL_DEBUGS("MediaOnAPrim") << "BEFORE: tex_idx = " << tex_idx
							  << " hasMedia = " << tep->hasMedia() << " : ";
	if (tep->getMediaData())
	{
		LL_CONT << ll_pretty_print_sd(tep->getMediaData()->asLLSD());
	}
	else
	{
		LL_CONT << "NULL MEDIA DATA";
	}
	LL_CONT << LL_ENDL;

	std::string previous_url;
	LLMediaEntry* mep = tep->getMediaData();
	if (mep)
	{
		// Save the "current url" from before the update so we can tell if
		// it changes.
		previous_url = mep->getCurrentURL();
	}

	if (merge)
	{
		tep->mergeIntoMediaData(media_data);
	}
	else
	{
		// XXX Question: what if the media data is undefined LLSD, but the
		// update we got above said that we have media flags??	Here we clobber
		// that, assuming the data from the service is more up-to-date.
		tep->updateMediaData(media_data);
	}

	mep = tep->getMediaData();
	if (mep)
	{
		bool update_from_self = !ignore_agent &&
			LLTextureEntry::getAgentIDFromMediaVersionString(getMediaURL()) == gAgentID;
		viewer_media_t media_impl =
		LLViewerMedia::updateMediaImpl(mep, previous_url, update_from_self);
		addMediaImpl(media_impl, tex_idx);
	}
	else
	{
		removeMediaImpl(tex_idx);
	}

	LL_DEBUGS("MediaOnAPrim") << "AFTER: tex_idx = " << tex_idx
							  << " hasMedia = " << tep->hasMedia() << " : ";
	if (tep->getMediaData())
	{
		LL_CONT << ll_pretty_print_sd(tep->getMediaData()->asLLSD());
	}
	else
	{
		LL_CONT << "NULL MEDIA DATA";
	}
	LL_CONT << LL_ENDL;
}

void LLVOVolume::mediaNavigateBounceBack(U8 tex_idx)
{
	// Find the media entry for this navigate
	const LLMediaEntry* mep = NULL;
	viewer_media_t impl = getMediaImpl(tex_idx);
	LLTextureEntry* tep = getTE(tex_idx);
	if (tep)
	{
		mep = tep->getMediaData();
	}

	if (mep && impl)
	{
		std::string url = mep->getCurrentURL();
		// Look for a ":", if not there, assume "http://"
		if (!url.empty() && url.find(':') == std::string::npos)
		{
			url = "http://" + url;
		}
		// If the url we are trying to "bounce back" to either empty or not
		// allowed by the whitelist, try the home url. If *that* does not work,
		// set the media as failed and unload it
		if (url.empty() || !mep->checkCandidateUrl(url))
		{
			url = mep->getHomeURL();
			// Look for a ":", if not there, assume "http://"
			if (!url.empty() && url.find(':') == std::string::npos)
			{
				url = "http://" + url;
			}
		}
		if (url.empty() || !mep->checkCandidateUrl(url))
		{
			// The url to navigate back to is not good, and we have nowhere
			// else to go.
			llwarns << "FAILED to bounce back URL \"" << url
					<< "\" -- unloading impl" << llendl;
			impl->setMediaFailed(true);
		}
		else if (impl->getCurrentMediaURL() != url)
		{
			// Okay, navigate now
			llinfos << "bouncing back to URL: " << url << llendl;
			impl->navigateTo(url, "", false, true);
		}
	}
}

bool LLVOVolume::hasMediaPermission(const LLMediaEntry* media_entry,
									MediaPermType perm_type)
{
	// NOTE: This logic ALMOST duplicates the logic in the server (in
	// particular, in llmediaservice.cpp).
	if (!media_entry) return false; // XXX should we assert here?

	// The agent has permissions if:
	// - world permissions are on, or
	// - group permissions are on, and agent_id is in the group, or
	// - agent permissions are on, and agent_id is the owner

	// *NOTE: We *used* to check for modify permissions here (i.e. permissions
	// were granted if permModify() was true).  However, this doesn't make
	// sense in the viewer: we do not want to show controls or allow
	// interaction if the author has deemed it so. See DEV-42115.

	U8 media_perms = perm_type ==
		MEDIA_PERM_INTERACT ? media_entry->getPermsInteract()
							: media_entry->getPermsControl();

	// World permissions
	if ((media_perms & LLMediaEntry::PERM_ANYONE) != 0)
	{
		return true;
	}
	// Group permissions
	else if ((media_perms & LLMediaEntry::PERM_GROUP) != 0)
	{
		LLPermissions* obj_perm;
		obj_perm = gSelectMgr.findObjectPermissions(this);
		if (obj_perm && gAgent.isInGroup(obj_perm->getGroup()))
		{
			return true;
		}
	}
	// Owner permissions
	else if ((media_perms & LLMediaEntry::PERM_OWNER) != 0 && permYouOwner())
	{
		return true;
	}

	return false;
}

void LLVOVolume::mediaNavigated(LLViewerMediaImpl* impl,
								LLPluginClassMedia* plugin,
								std::string new_location)
{
	bool block_navigation = false;
	// FIXME: if/when we allow the same media impl to be used by multiple
	// faces, the logic here will need to be fixed to deal with multiple face
	// indices.
	S32 face_index = getFaceIndexWithMediaImpl(impl, -1);

	// Find the media entry for this navigate
	LLMediaEntry* mep = NULL;
	LLTextureEntry* tep = getTE(face_index);
	if (tep)
	{
		mep = tep->getMediaData();
	}

	if (mep)
	{
		if (!mep->checkCandidateUrl(new_location))
		{
			block_navigation = true;
		}
		if (!block_navigation && !hasMediaPermission(mep, MEDIA_PERM_INTERACT))
		{
			block_navigation = true;
		}
	}
	else
	{
		llwarns_sparse << "Could not find media entry" << llendl;
	}

	if (block_navigation)
	{
		llinfos << "blocking navigate to URI " << new_location << llendl;

		// "bounce back" to the current URL from the media entry
		mediaNavigateBounceBack(face_index);
	}
	else if (sObjectMediaNavigateClient)
	{

		LL_DEBUGS("MediaOnAPrim") << "broadcasting navigate with URI "
								  << new_location << LL_ENDL;

		sObjectMediaNavigateClient->navigate(new LLMediaDataClientObjectImpl(this,
																			 false),
											 face_index, new_location);
	}
}

void LLVOVolume::mediaEvent(LLViewerMediaImpl* impl,
							LLPluginClassMedia* plugin,
							LLViewerMediaObserver::EMediaEvent event)
{
	switch (event)
	{
		case LLViewerMediaObserver::MEDIA_EVENT_LOCATION_CHANGED:
		{
			switch (impl->getNavState())
			{
				case LLViewerMediaImpl::MEDIANAVSTATE_FIRST_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a non-server-directed nav. It may need to be
					// broadcast or bounced back.
					mediaNavigated(impl, plugin, plugin->getLocation());
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS:
					// This navigate didn't change the current URL.
					LL_DEBUGS("MediaOnAPrim") << "NOT broadcasting navigate (spurious)"
											  << LL_ENDL;
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a server-directed nav. do not broadcast it.
					llinfos << "NOT broadcasting navigate (server-directed)"
							<< llendl;
					break;

				default:
					// This is a subsequent location-changed due to a redirect.
					// do not broadcast.
					llinfos << "NOT broadcasting navigate (redirect)"
							<< llendl;
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			switch (impl->getNavState())
			{
				case LLViewerMediaImpl::MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a non-server-directed nav. It may need to be
					// broadcast or bounced back.
					mediaNavigated(impl, plugin, plugin->getNavigateURI());
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS:
					// This navigate didn't change the current URL.
					LL_DEBUGS("MediaOnAPrim") << "NOT broadcasting navigate (spurious)"
											  << LL_ENDL;
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED:
					// This is the the navigate complete event from a
					// server-directed nav. do not broadcast it.
					llinfos << "NOT broadcasting navigate (server-directed)"
							<< llendl;
					break;

				default:
					// For all other states, the navigate should have been
					// handled by LOCATION_CHANGED events already.
					break;
			}
			break;
		}

		default:
			break;
	}
}

void LLVOVolume::sendMediaDataUpdate()
{
	if (!sObjectMediaClient)
	{
		return;
	}
	sObjectMediaClient->updateMedia(new LLMediaDataClientObjectImpl(this,
																	false));
}

void LLVOVolume::removeMediaImpl(S32 tex_idx)
{
	S32 media_count = mMediaImplList.size();
	if (tex_idx >= media_count || mMediaImplList[tex_idx].isNull())
	{
		return;
	}

	// Make the face referencing to mMediaImplList[tex_idx] to point back to
	// the old texture.
	if (mDrawable.notNull() && tex_idx < mDrawable->getNumFaces())
	{
		LLFace* facep = mDrawable->getFace(tex_idx);
		if (facep)
		{
			const LLUUID& id = mMediaImplList[tex_idx]->getMediaTextureID();
			LLViewerMediaTexture* media_tex =
				LLViewerTextureManager::findMediaTexture(id);
			if (media_tex)
			{
				media_tex->removeMediaFromFace(facep);
			}
		}
	}

	// Check if some other face(s) of this object reference(s)to this media
	// impl.
	S32 i;
	for (i = 0; i < media_count; ++i)
	{
		if (i != tex_idx && mMediaImplList[i] == mMediaImplList[tex_idx])
		{
			break;
		}
	}

	if (i == media_count)	// This object does not need this media impl.
	{
		mMediaImplList[tex_idx]->removeObject(this);
	}

	mMediaImplList[tex_idx] = NULL;
}

void LLVOVolume::addMediaImpl(LLViewerMediaImpl* media_implp, S32 tex_idx)
{
	if ((S32)mMediaImplList.size() < tex_idx + 1)
	{
		mMediaImplList.resize(tex_idx + 1);
	}

	if (mMediaImplList[tex_idx].notNull())
	{
		if (mMediaImplList[tex_idx] == media_implp)
		{
			return;
		}

		removeMediaImpl(tex_idx);
	}

	mMediaImplList[tex_idx] = media_implp;
	media_implp->addObject(this);

	// Add the face to show the media if it is in playing
	if (mDrawable.notNull())
	{
		LLFace* facep = NULL;
		if (tex_idx < mDrawable->getNumFaces())
		{
			facep = mDrawable->getFace(tex_idx);
		}
		if (facep)
		{
			LLViewerMediaTexture* media_texp =
				LLViewerTextureManager::findMediaTexture(mMediaImplList[tex_idx]->getMediaTextureID());
			if (media_texp)
			{
				media_texp->addMediaToFace(facep);
			}
		}
		else // The face is not available now, start media on this face later.
		{
			media_implp->setUpdated(true);
		}
	}
}

viewer_media_t LLVOVolume::getMediaImpl(U8 face_id) const
{
	if (face_id < mMediaImplList.size())
	{
		return mMediaImplList[face_id];
	}
	return NULL;
}

F64 LLVOVolume::getTotalMediaInterest() const
{
	// If this object is currently focused, this object has "high" interest
	if (LLViewerMediaFocus::getInstance()->getFocusedObjectID() == getID())
	{
		return F64_MAX;
	}

	F64 interest = -1.0;  // means not interested;

	// If this object is selected, this object has "high" interest, but since
	// there can be more than one, we still add in calculated impl interest
	// XXX Sadly, 'contains()' doesn't take a const :(
	if (gSelectMgr.getSelection()->contains(const_cast<LLVOVolume*>(this)))
	{
		interest = F64_MAX * 0.5;
	}

	for (S32 i = 0, end = getNumTEs(); i < end; ++i)
	{
		const viewer_media_t& impl = getMediaImpl(i);
		if (impl.notNull())
		{
			if (interest == -1.0)
			{
				interest = 0.0;
			}
			interest += impl->getInterest();
		}
	}
	return interest;
}

S32 LLVOVolume::getFaceIndexWithMediaImpl(const LLViewerMediaImpl* media_impl,
										  S32 start_face_id)
{
	for (S32 face_id = start_face_id + 1, end = mMediaImplList.size();
		 face_id < end; ++face_id)
	{
		if (mMediaImplList[face_id] == media_impl)
		{
			return face_id;
		}
	}
	return -1;
}

void LLVOVolume::setLightTextureID(const LLUUID& id)
{
	// Same as mLightTexture, but initializes if necessary:
	LLViewerTexture* old_texturep = getLightTexture();

	if (id.notNull())
	{
		if (!hasLightTexture())
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE, true,
								   true);
		}
		else if (old_texturep)
		{
			old_texturep->removeVolume(LLRender::LIGHT_TEX, this);
		}
		LLLightImageParams* param_block = getLightImageParams();
		if (param_block && param_block->getLightTexture() != id)
		{
			param_block->setLightTexture(id);
			parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
		}
		// New light texture
		LLViewerTexture* new_texturep = getLightTexture();
		if (new_texturep)
		{
			new_texturep->addVolume(LLRender::LIGHT_TEX, this);
		}
	}
	else if (hasLightTexture())
	{
		if (old_texturep)
		{
			old_texturep->removeVolume(LLRender::LIGHT_TEX, this);
		}
		setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE, false, true);
		parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
		mLightTexture = NULL;
	}
}

void LLVOVolume::setSpotLightParams(const LLVector3& params)
{
	LLLightImageParams* param_block = getLightImageParams();
	if (param_block && param_block->getParams() != params)
	{
		param_block->setParams(params);
		parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
	}
}

void LLVOVolume::setIsLight(bool is_light)
{
	if (is_light != getIsLight())
	{
		if (is_light)
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, true, true);
			// Add it to the pipeline light set.
			gPipeline.setLight(mDrawable, true);
		}
		else
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, false, true);
			// Not a light. Remove it from the pipeline light set.
			gPipeline.setLight(mDrawable, false);
		}
	}
}

void LLVOVolume::setLightLinearColor(const LLColor3& color)
{
	LLLightParams* param_block = getLightParams();
	if (!param_block || param_block->getLinearColor() == color)
	{
		return;
	}
	param_block->setLinearColor(LLColor4(color,
										 param_block->getLinearColor().mV[3]));
	parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	gPipeline.markTextured(mDrawable);
	mFaceMappingChanged = true;
}

void LLVOVolume::setLightIntensity(F32 intensity)
{
	LLLightParams* param_block = getLightParams();
	if (!param_block || param_block->getLinearColor().mV[3] == intensity)
	{
		return;
	}
	param_block->setLinearColor(LLColor4(LLColor3(param_block->getLinearColor()),
										 intensity));
	parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
}

void LLVOVolume::setLightRadius(F32 radius)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getRadius() != radius)
	{
		param_block->setRadius(radius);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

void LLVOVolume::setLightFalloff(F32 falloff)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getFalloff() != falloff)
	{
		param_block->setFalloff(falloff);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

void LLVOVolume::setLightCutoff(F32 cutoff)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getCutoff() != cutoff)
	{
		param_block->setCutoff(cutoff);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

bool LLVOVolume::getIsLight() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT);
}

LLColor3 LLVOVolume::getLightLinearBaseColor() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? LLColor3(param_block->getLinearColor())
					   : LLColor3::white;
}

LLColor3 LLVOVolume::getLightLinearColor() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? LLColor3(param_block->getLinearColor()) *
								  param_block->getLinearColor().mV[3]
					   : LLColor3::white;
}

LLColor3 LLVOVolume::getLightSRGBColor() const
{
	LLColor3 ret = getLightLinearColor();
	ret = srgbColor3(ret);
	return ret;
}

const LLUUID& LLVOVolume::getLightTextureID() const
{
	const LLLightImageParams* param_block = getLightImageParams();
	return param_block ? param_block->getLightTexture() : LLUUID::null;
}

LLVector3 LLVOVolume::getSpotLightParams() const
{
	const LLLightImageParams* param_block = getLightImageParams();
	return param_block ? param_block->getParams() : LLVector3::zero;
}

void LLVOVolume::updateSpotLightPriority()
{
	if (gCubeSnapshot)
	{
		return;
	}

	LLVector3 pos = mDrawable->getPositionAgent();
	LLVector3 at(0.f, 0.f, -1.f);
	at *= getRenderRotation();

	F32 r = getLightRadius() * 0.5f;

	pos += at * r;

	at = gViewerCamera.getAtAxis();

	pos -= at * r;

	mSpotLightPriority = gPipeline.calcPixelArea(pos, LLVector3(r, r, r),
												 gViewerCamera);

	if (mLightTexture.notNull())
	{
		mLightTexture->addTextureStats(mSpotLightPriority);
		mLightTexture->setBoostLevel(LLGLTexture::BOOST_CLOUDS);
	}
}

bool LLVOVolume::isLightSpotlight() const
{
	const LLLightImageParams* params = getLightImageParams();
	return params && params->isLightSpotlight();
}

LLViewerTexture* LLVOVolume::getLightTexture()
{
	const LLUUID& id = getLightTextureID();
	if (id.isNull())
	{
		mLightTexture = NULL;
	}
	else if (mLightTexture.isNull() || id != mLightTexture->getID())
	{
		mLightTexture =
			LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
													  LLGLTexture::BOOST_ALM);
	}

	return mLightTexture;
}

F32 LLVOVolume::getLightIntensity() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getLinearColor().mV[3] : 1.f;
}

F32 LLVOVolume::getLightRadius() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getRadius() : 0.f;
}

F32 LLVOVolume::getLightFalloff(F32 fudge_factor) const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getFalloff() * fudge_factor : 0.f;
}

F32 LLVOVolume::getLightCutoff() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getCutoff() : 0.f;
}

bool LLVOVolume::isReflectionProbe() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_REFLECTION_PROBE);
}

bool LLVOVolume::setIsReflectionProbe(bool is_probe)
{
	bool changed = is_probe != isReflectionProbe();
	if (changed)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_REFLECTION_PROBE,
							   is_probe, true);
	}
	updateReflectionProbePtr();
	return changed;
}

bool LLVOVolume::setReflectionProbeAmbiance(F32 ambiance)
{
	LLReflectionProbeParams* paramsp = getReflectionProbeParams();
	if (paramsp && paramsp->getAmbiance() != ambiance)
	{
		paramsp->setAmbiance(ambiance);
		parameterChanged(LLNetworkData::PARAMS_REFLECTION_PROBE, true);
		return true;
	}
	return false;
}

bool LLVOVolume::setReflectionProbeNearClip(F32 near_clip)
{
	LLReflectionProbeParams* paramsp = getReflectionProbeParams();
	if (paramsp && paramsp->getClipDistance() != near_clip)
	{
		paramsp->setClipDistance(near_clip);
		parameterChanged(LLNetworkData::PARAMS_REFLECTION_PROBE, true);
		return true;
	}
	return false;
}

bool LLVOVolume::setReflectionProbeIsBox(bool is_box)
{
	LLReflectionProbeParams* paramsp = getReflectionProbeParams();
	if (paramsp && paramsp->getIsBox() != is_box)
	{
		paramsp->setIsBox(is_box);
		parameterChanged(LLNetworkData::PARAMS_REFLECTION_PROBE, true);
		return true;
	}
	return false;
}

bool LLVOVolume::setReflectionProbeIsDynamic(bool is_dynamic)
{
	LLReflectionProbeParams* paramsp = getReflectionProbeParams();
	if (paramsp && paramsp->getIsDynamic() != is_dynamic)
	{
		paramsp->setIsDynamic(is_dynamic);
		parameterChanged(LLNetworkData::PARAMS_REFLECTION_PROBE, true);
		return true;
	}
	return false;
}

F32 LLVOVolume::getReflectionProbeAmbiance() const
{
	const LLReflectionProbeParams* paramsp =
		(const LLReflectionProbeParams*)getReflectionProbeParams();
	return paramsp ? paramsp->getAmbiance() : 0.f;
}

F32 LLVOVolume::getReflectionProbeNearClip() const
{
	const LLReflectionProbeParams* paramsp =
		(const LLReflectionProbeParams*)getReflectionProbeParams();
	return paramsp ? paramsp->getClipDistance() : 0.f;
}

bool LLVOVolume::getReflectionProbeIsBox() const
{
	const LLReflectionProbeParams* paramsp =
		(const LLReflectionProbeParams*)getReflectionProbeParams();
	return paramsp && paramsp->getIsBox();
}

bool LLVOVolume::getReflectionProbeIsDynamic() const
{
	const LLReflectionProbeParams* paramsp =
		(const LLReflectionProbeParams*)getReflectionProbeParams();
	return paramsp && paramsp->getIsDynamic();
}

U32 LLVOVolume::getVolumeInterfaceID() const
{
	return mVolumeImpl ? mVolumeImpl->getID() : 0;
}

bool LLVOVolume::isFlexible() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE);
}

bool LLVOVolume::isSculpted() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT);
}

bool LLVOVolume::isMesh() const
{
	if (isSculpted())
	{
		const LLSculptParams* params = getSculptParams();
		if (params)
		{
			U8 sculpt_type = params->getSculptType();
			if ((sculpt_type & LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH)
			{
				return true;
			}
		}
	}

	return false;
}

bool LLVOVolume::hasLightTexture() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE);
}

bool LLVOVolume::isVolumeGlobal() const
{
	return mVolumeImpl ? mVolumeImpl->isVolumeGlobal()
					   : mRiggedVolume.notNull();
}

bool LLVOVolume::canBeFlexible() const
{
	U8 path = getVolume()->getParams().getPathParams().getCurveType();
	return path == LL_PCODE_PATH_FLEXIBLE || path == LL_PCODE_PATH_LINE;
}

bool LLVOVolume::setIsFlexible(bool is_flexible)
{
	bool res = false;
	bool was_flexible = isFlexible();
	LLVolumeParams volume_params;
	if (is_flexible)
	{
		if (!was_flexible)
		{
			volume_params = getVolume()->getParams();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			volume_params.setType(profile_and_hole, LL_PCODE_PATH_FLEXIBLE);
			res = true;
			setFlags(FLAGS_USE_PHYSICS, false);
			setFlags(FLAGS_PHANTOM, true);
			setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, true, true);
			if (mDrawable.notNull())
			{
				mDrawable->makeActive();
			}
		}
	}
	else if (was_flexible)
	{
		volume_params = getVolume()->getParams();
		U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
		volume_params.setType(profile_and_hole, LL_PCODE_PATH_LINE);
		res = true;
		setFlags(FLAGS_PHANTOM, false);
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, false, true);
	}
	if (res)
	{
		res = setVolume(volume_params, 1);
		if (res)
		{
			markForUpdate();
		}
	}
	return res;
}

const LLMeshSkinInfo* LLVOVolume::getSkinInfo() const
{
	return getVolume() ? mSkinInfo : nullptr;
}

bool LLVOVolume::isRiggedMesh() const
{
	return isMesh() && getSkinInfo();
}

U32 LLVOVolume::getExtendedMeshFlags() const
{
	const LLExtendedMeshParams* param_blockp = getExtendedMeshParams();
	return param_blockp ? param_blockp->getFlags() : 0;
}

void LLVOVolume::onSetExtendedMeshFlags(U32 flags)
{
	if (mDrawable.notNull())
	{
		// Need to trigger rebuildGeom(), which is where puppet avatars get
		// created/removed.
		getRootEdit()->recursiveMarkForUpdate();
	}
	
	if (isAttachment())
	{
		LLVOAvatar* avatarp = getAvatarAncestor();
		if (avatarp)
		{
			updateVisualComplexity();
			avatarp->updateAttachmentOverrides();
		}
	}
}

void LLVOVolume::setExtendedMeshFlags(U32 flags)
{
	U32 curr_flags = getExtendedMeshFlags();
	if (curr_flags != flags)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_EXTENDED_MESH, true,
							   true);
		LLExtendedMeshParams* param_blockp = getExtendedMeshParams();
		if (param_blockp)
		{
			param_blockp->setFlags(flags);
		}
		parameterChanged(LLNetworkData::PARAMS_EXTENDED_MESH, true);
		onSetExtendedMeshFlags(flags);
	}
}

bool LLVOVolume::canBeAnimatedObject() const
{
	F32 est_tris = recursiveGetEstTrianglesMax();
	F32 max_tris = getAnimatedObjectMaxTris();
	if (est_tris < 0 || est_tris > getAnimatedObjectMaxTris())
	{
		LL_DEBUGS("Mesh") << "Estimated triangles amount " << est_tris
						  << " out of limit 0-" << max_tris << LL_ENDL;
		return false;
	}
	return true;
}

bool LLVOVolume::isAnimatedObject() const
{
	LLVOVolume* root_volp = (LLVOVolume*)getRootEdit();
	return root_volp &&
		   (root_volp->getExtendedMeshFlags() &
			LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG) != 0;
}

// Called any time parenting changes for a volume. Update flags and puppet
// avatar accordingly. This is called after parent has been changed to
// new_parent.
//virtual
void LLVOVolume::onReparent(LLViewerObject* old_parentp,
							LLViewerObject* new_parentp)
{
	LLVOVolume* old_volp = old_parentp ? old_parentp->asVolume() : NULL;

	// AXON: depending on whether animated objects can be attached, we may want
	// to include or remove the isAvatar() check.
	if (new_parentp && !new_parentp->isAvatar())
	{
		if (mPuppetAvatar.notNull())
		{
			mPuppetAvatar->markForDeath();
			mPuppetAvatar = NULL;
		}
	}
	if (old_volp && old_volp->isAnimatedObject())
	{
		LLVOAvatarPuppet* puppetp = old_volp->getPuppetAvatar();
		if (puppetp)
		{
			// We have been removed from an animated object, need to do cleanup
			puppetp->updateAttachmentOverrides();
			puppetp->updateAnimations();
		}
	}
}

// This needs to be called after onReparent(), because mChildList is not
// updated until the end of addChild()
//virtual
void LLVOVolume::afterReparent()
{
	if (isAnimatedObject())
	{
		LLVOAvatarPuppet* puppetp = getPuppetAvatar();
		if (puppetp)
		{
#if 0
			puppetp->rebuildAttachmentOverrides();
#endif
			puppetp->updateAnimations();
		}
	}
}

//virtual
void LLVOVolume::updateRiggingInfo()
{
	if (!isRiggedMesh() || (mLOD != 3 && mLOD <= mLastRiggingInfoLOD))
	{
		return;
	}

	const LLMeshSkinInfo* skinp = getSkinInfo();
	if (!skinp) return;

	LLVOAvatar* avatarp = getAvatar();
	if (!avatarp) return;

	LLVolume* volp = getVolume();
	if (!volp) return;

	// Rigging info may need update
	mJointRiggingInfoTab.clear();
	for (S32 i = 0, count = volp->getNumVolumeFaces(); i < count; ++i)
	{
		LLVolumeFace& vol_face = volp->getVolumeFace(i);
		LLSkinningUtil::updateRiggingInfo(skinp, avatarp, vol_face);
		if (vol_face.mJointRiggingInfoTab.size())
		{
			mJointRiggingInfoTab.merge(vol_face.mJointRiggingInfoTab);
		}
	}

	mLastRiggingInfoLOD = mLOD;
}

void LLVOVolume::generateSilhouette(LLSelectNode* nodep,
									const LLVector3& view_point)
{
	LLVolume* volp = getVolume();
	if (volp && getRegion())
	{
		LLVector3 view_vector;
		view_vector = view_point;

		// Transform view vector into volume space
		view_vector -= getRenderPosition();
#if 0
		mDrawable->mDistanceWRTCamera = view_vector.length();
#endif
		LLQuaternion world_rot = getRenderRotation();
		view_vector = view_vector * ~world_rot;
		if (!isVolumeGlobal())
		{
			LLVector3 obj_scale = getScale();
			LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX],
									1.f / obj_scale.mV[VY],
									1.f / obj_scale.mV[VZ]);
			view_vector.scaleVec(inv_obj_scale);
		}

		updateRelativeXform();
		LLMatrix4 trans_mat = mRelativeXform;
		if (mDrawable->isStatic())
		{
			trans_mat.translate(getRegion()->getOriginAgent());
		}

		volp->generateSilhouetteVertices(nodep->mSilhouetteVertices,
										 nodep->mSilhouetteNormals,
										 view_vector, trans_mat,
										 mRelativeXformInvTrans,
										 nodep->getTESelectMask());

		nodep->mSilhouetteGenerated = true;
	}
}

void LLVOVolume::deleteFaces()
{
	S32 face_count = mNumFaces;
	if (mDrawable.notNull())
	{
		mDrawable->deleteFaces(0, face_count);
	}

	mNumFaces = 0;
}

void LLVOVolume::updateRadius()
{
	if (mDrawable.notNull())
	{
		mVObjRadius = getScale().length();
		mDrawable->setRadius(mVObjRadius);
	}
}

bool LLVOVolume::isAttachment() const
{
	return mAttachmentState != 0;
}

bool LLVOVolume::isHUDAttachment() const
{
	// *NOTE: we assume hud attachment points are in defined range since this
	// range is constant for backwards compatibility reasons this is probably a
	// reasonable assumption to make.
	S32 attachment_id = ATTACHMENT_ID_FROM_STATE(mAttachmentState);
	return attachment_id >= 31 && attachment_id <= 38;
}

const LLMatrix4& LLVOVolume::getRenderMatrix() const
{
	if (mDrawable->isActive() && !mDrawable->isRoot())
	{
		return mDrawable->getParent()->getWorldMatrix();
	}
	return mDrawable->getWorldMatrix();
}

// Returns a base cost and adds textures to passed in set. Total cost is
// returned value + 5 * size of the resulting set. Cannot include cost of
// textures, as they may be re-used in linked children, and cost should only be
// increased for unique textures  -Nyx
/**************************************************************************
 * The calculation in this method should not be modified by third party
 * viewers, since it is used to limit rendering and should be uniform for
 * everyone. If you have suggested improvements, submit them to the official
 * viewer for consideration.
 *************************************************************************/
U32 LLVOVolume::getRenderCost(texture_cost_t& textures) const
{
	if (mDrawable.isNull()) return 0;

	// Per-prim costs, determined experimentally
	constexpr U32 ARC_PARTICLE_COST = 1;
	// Default value
	constexpr U32 ARC_PARTICLE_MAX = 2048;
	// Multiplier for texture resolution - performance tested
	constexpr F32 ARC_TEXTURE_COST_BY_128 = 16.f / 128.f;
	// Cost for light-producing prims
	constexpr U32 ARC_LIGHT_COST = 500;
	// Cost per media-enabled face
	constexpr U32 ARC_MEDIA_FACE_COST = 1500;

	// Per-prim multipliers (tested based on performances)
	constexpr F32 ARC_GLOW_MULT = 1.5f;
	constexpr F32 ARC_BUMP_MULT = 1.25f;
	constexpr F32 ARC_FLEXI_MULT = 5.f;
	constexpr F32 ARC_SHINY_MULT = 1.6f;
	constexpr F32 ARC_INVISI_COST = 1.2f;
	constexpr F32 ARC_WEIGHTED_MESH = 1.2f;
	// Tested to have negligible impact
	constexpr F32 ARC_PLANAR_COST = 1.f;
	constexpr F32 ARC_ANIM_TEX_COST = 4.f;
	// 4x max based on performance
	constexpr F32 ARC_ALPHA_COST = 4.f;

	// Note: this object might not have a volume (e.g. if it is an avatar).
	U32 num_triangles = 0;
	LLVolume* volp = getVolume();
	if (volp)
	{
		LLMeshCostData* costs = getCostData();
		if (costs)
		{
			if (isAnimatedObject() && isRiggedMesh())
			{
				// AXON: scaling here is to make animated object versus non
				// animated object ARC proportional to the corresponding
				// calculations for streaming cost.
				 num_triangles = (U32)(ANIMATED_OBJECT_COST_PER_KTRI * 0.001f *
									   costs->getEstTrisForStreamingCost() /
									   0.06f);
			}
			else
			{
				 F32 radius = getScale().length() * 0.5f;
				 num_triangles = costs->getRadiusWeightedTris(radius);
			}
		}
	}

	if (num_triangles <= 0)
	{
		num_triangles = 4;
	}

	if (volp && isSculpted() && !isMesh())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			const LLUUID& sculpt_id = sculpt_params->getSculptTexture();
			if (!textures.count(sculpt_id))
			{
				LLViewerFetchedTexture* tex =
					LLViewerTextureManager::getFetchedTexture(sculpt_id);
				if (tex)
				{
					S32 cost = 256 +
							  (S32)(ARC_TEXTURE_COST_BY_128 *
									(F32)(tex->getFullHeight() +
										  tex->getFullWidth()));
					textures[sculpt_id] = cost;
				}
			}
		}
	}

	// These are multipliers flags: do not add per-face.
	bool invisi = false;
	bool shiny = false;
	bool glow = false;
	bool alpha = false;
	bool animtex = false;
	bool bump = false;
	bool planar = false;
	// Per media-face shame
	U32 media_faces = 0;
	for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
	{
		const LLFace* face = mDrawable->getFace(i);
		if (!face) continue;

		const LLViewerTexture* texp = face->getTexture();
		if (texp)
		{
			const LLUUID& tex_id = texp->getID();
			if (!textures.count(tex_id))
			{
				S32 cost = 0;
				S8 type = texp->getType();
				if (type == LLViewerTexture::FETCHED_TEXTURE ||
					type == LLViewerTexture::LOD_TEXTURE)
				{
					const LLViewerFetchedTexture* ftexp =
						(LLViewerFetchedTexture*)texp;
					if (ftexp->getFTType() == FTT_LOCAL_FILE &&
						(tex_id == IMG_ALPHA_GRAD_2D ||
						 tex_id == IMG_ALPHA_GRAD))
					{
						// These two textures appear to switch between each
						// other, but are of different sizes (4x256 and
						// 256x256). Hard-code cost from larger one to not
						// cause random complexity changes.
						cost = 320;
					}
				}
				if (cost == 0)
				{
					cost = 256 +
						  (S32)(ARC_TEXTURE_COST_BY_128 *
								(F32)(texp->getFullHeight() +
									  texp->getFullWidth()));
				}
				textures[tex_id] = cost;
			}
		}

		if (face->isInAlphaPool())
		{
			alpha = true;
		}
		else if (texp && texp->getPrimaryFormat() == GL_ALPHA)
		{
			invisi = true;
		}

		if (face->hasMedia())
		{
			++media_faces;
		}

		animtex |= face->mTextureMatrix != NULL;

		const LLTextureEntry* te = face->getTextureEntry();
		if (te)
		{
			bump |= te->getBumpmap() != 0;
			shiny |= te->getShiny() != 0;
			glow |= te->hasGlow();
			planar |= te->getTexGen() != 0;
		}
	}

	// Shame currently has the "base" cost of 1 point per 15 triangles, min 2.
	F32 shame = num_triangles * 5.f;
	shame = shame < 2.f ? 2.f : shame;

	// Multiply by per-face modifiers
	if (planar)
	{
		shame *= ARC_PLANAR_COST;
	}

	if (animtex)
	{
		shame *= ARC_ANIM_TEX_COST;
	}

	if (alpha)
	{
		shame *= ARC_ALPHA_COST;
	}

	if (invisi)
	{
		shame *= ARC_INVISI_COST;
	}

	if (glow)
	{
		shame *= ARC_GLOW_MULT;
	}

	if (bump)
	{
		shame *= ARC_BUMP_MULT;
	}

	if (shiny)
	{
		shame *= ARC_SHINY_MULT;
	}

	if (isRiggedMesh())
	{
		shame *= ARC_WEIGHTED_MESH;
	}

	if (isFlexible())
	{
		shame *= ARC_FLEXI_MULT;
	}

	// Add additional costs
	if (isParticleSource())
	{
		const LLPartSysData* part_sys_data = &(mPartSourcep->mPartSysData);
		const LLPartData* part_data = &(part_sys_data->mPartData);
		U32 num_particles = (U32)(part_sys_data->mBurstPartCount *
								  llceil(part_data->mMaxAge /
										 part_sys_data->mBurstRate));
		num_particles = num_particles > ARC_PARTICLE_MAX ? ARC_PARTICLE_MAX
														 : num_particles;
		F32 part_size = (llmax(part_data->mStartScale[0],
							   part_data->mEndScale[0]) +
						 llmax(part_data->mStartScale[1],
							   part_data->mEndScale[1])) * 0.5f;
		shame += num_particles * part_size * ARC_PARTICLE_COST;
	}

	if (getIsLight())
	{
		shame += ARC_LIGHT_COST;
	}

	if (media_faces)
	{
		shame += media_faces * ARC_MEDIA_FACE_COST;
	}

	// Streaming cost for animated objects includes a fixed cost per linkset.
	// Add a corresponding charge here translated into triangles, but not
	// weighted by any graphics properties.
	if (isAnimatedObject() && isRootEdit())
	{
		shame += ANIMATED_OBJECT_BASE_COST * 5.f / 0.06f;
	}

	return (U32)shame;
}

//virtual
F32 LLVOVolume::getEstTrianglesMax() const
{
	LLVolume* volp = getVolume();
	if (volp && isMesh())
	{
		return gMeshRepo.getEstTrianglesMax(volp->getParams().getSculptID());
	}
	return 0.f;
}

//virtual
F32 LLVOVolume::getEstTrianglesStreamingCost() const
{
	LLVolume* volp = getVolume();
	if (volp && isMesh())
	{
		return gMeshRepo.getEstTrianglesStreamingCost(volp->getParams().getSculptID());
	}
	return 0.f;
}

F32 LLVOVolume::getStreamingCost(S32* bytes, S32* visible_bytes,
								 F32* unscaled_value) const
{
	LLMeshCostData* cost_data = getCostData();
	if (!cost_data)
	{
		return 0.f;
	}

	F32 cost = 0.f;
	bool animated = isAnimatedObject();
	if (animated && isRootEdit())
	{
		// Root object of an animated object has this to account for skeleton
		// overhead.
		cost = ANIMATED_OBJECT_BASE_COST;
	}

	F32 radius = getScale().length() * 0.5f;

	if (isMesh() && animated && isRiggedMesh())
	{
		cost += cost_data->getTriangleBasedStreamingCost();
	}
	else
	{
		cost += cost_data->getRadiusBasedStreamingCost(radius);
	}

	if (bytes)
	{
		*bytes = cost_data->getSizeTotal();
	}

	if (visible_bytes)
	{
		*visible_bytes = cost_data->getSizeByLOD(mLOD);
	}

	if (unscaled_value)
	{
		*unscaled_value = cost_data->getRadiusWeightedTris(radius);
	}

	return cost;
}

LLMeshCostData* LLVOVolume::getCostData() const
{
	if (mCostData.notNull())
	{
		return mCostData.get();
	}

	LLVolume* volp = getVolume();
	if (volp)	// Paranoia
	{
		if (isMesh())
		{
			mCostData = gMeshRepo.getCostData(volp->getParams().getSculptID());
		}
		else
		{
			mCostData = new LLMeshCostData();
			S32 counts[4];
			volp->getLoDTriangleCounts(counts);
			if (!mCostData->init(counts[0] * 10, counts[1] * 10,
								 counts[2] * 10, counts[3] * 10))
			{
				mCostData = NULL;
			}
		}
	}

	return mCostData.get();
}

#if 0	// Not yet implemented
//static
void LLVOVolume::updateRenderComplexity()
{
	mRenderComplexity_last = mRenderComplexity_current;
	mRenderComplexity_current = 0;
}
#endif

U32 LLVOVolume::getTriangleCount(S32* vcount) const
{
	LLVolume* volp = getVolume();
	return volp ? volp->getNumTriangles(vcount) : 0;
}

U32 LLVOVolume::getHighLODTriangleCount()
{
	U32 ret = 0;

	LLVolume* volp = getVolume();

	if (!isSculpted())
	{
		LLVolume* refp = gVolumeMgrp->refVolume(volp->getParams(), 3);
		if (refp)
		{
			ret = refp->getNumTriangles();
			gVolumeMgrp->unrefVolume(refp);
		}
	}
	else if (isMesh())
	{
		LLVolume* refp = gVolumeMgrp->refVolume(volp->getParams(), 3);
		if (refp)
		{
			if (!refp->isMeshAssetLoaded() || !refp->getNumVolumeFaces())
			{
				gMeshRepo.loadMesh(this, volp->getParams(), LLModel::LOD_HIGH);
			}
			ret = refp->getNumTriangles();
			gVolumeMgrp->unrefVolume(refp);
		}
	}
	else
	{
		// Default sculpts have a constant number of triangles: 31 rows of 31
		// columns of quads for a 32x32 vertex patch.
		ret = 31 * 2 * 31;
	}

	return ret;
}

//virtual
void LLVOVolume::parameterChanged(U16 param_type, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, local_origin);
}

//virtual
void LLVOVolume::parameterChanged(U16 param_type, LLNetworkData* datap,
								  bool in_use, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, datap, in_use, local_origin);
	if (mVolumeImpl)
	{
		mVolumeImpl->onParameterChanged(param_type, datap, in_use,
										local_origin);
	}
	if (!local_origin && param_type == LLNetworkData::PARAMS_EXTENDED_MESH)
	{
		U32 extended_mesh_flags = getExtendedMeshFlags();
		bool enabled = (extended_mesh_flags &
						LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG) != 0;
		// AXON: this is kind of a guess. Better if we could compare the before
		// and after flags directly. What about cases where there is no puppet
		// avatar for optimization reasons ?
		bool was_enabled = getPuppetAvatar() != NULL;
		if (enabled != was_enabled)
		{
			onSetExtendedMeshFlags(extended_mesh_flags);
		}
	}
	if (mDrawable.notNull())
	{
		bool is_light = getIsLight();
		if (is_light != mDrawable->isState(LLDrawable::LIGHT))
		{
			gPipeline.setLight(mDrawable, is_light);
		}
	}

	updateReflectionProbePtr();
}

void LLVOVolume::updateReflectionProbePtr()
{
	if (!gUsePBRShaders || !isReflectionProbe())
	{
		mReflectionProbe = NULL;
		return;
	}
	if (mReflectionProbe.isNull())
	{
		mReflectionProbe =
			gPipeline.mReflectionMapManager.registerViewerObject(this);
	}
}

void LLVOVolume::setSelected(bool sel)
{
	LLViewerObject::setSelected(sel);
	if (isAnimatedObject())
	{
		getRootEdit()->recursiveMarkForUpdate();
	}
	else
	{
		markForUpdate();
	}
}

void LLVOVolume::updateSpatialExtents(LLVector4a&, LLVector4a&)
{
}

F32 LLVOVolume::getBinRadius()
{
	static LLCachedControl<bool> new_bin_radius(gSavedSettings,
												"UseNewBinRadiusCompute");
	static LLCachedControl<S32> size_factor(gSavedSettings,
											"OctreeStaticObjectSizeFactor");
	static LLCachedControl<S32> att_size_factor(gSavedSettings,
												"OctreeAttachmentSizeFactor");
	static LLCachedControl<LLVector3> dist_factor(gSavedSettings,
												  "OctreeDistanceFactor");
	static LLCachedControl<LLVector3> alpha_factor(gSavedSettings,
												   "OctreeAlphaDistanceFactor");

	bool shrink_wrap = mShouldShrinkWrap || mDrawable->isAnimating();
	bool alpha_wrap = false;
	if (!isHUDAttachment() &&
		(!new_bin_radius ||
		 mDrawable->mDistanceWRTCamera < ((LLVector3)alpha_factor)[0]))
	{
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* face = mDrawable->getFace(i);
			if (face && face->isInAlphaPool() && !face->canRenderAsMask())
			{
				alpha_wrap = true;
				break;
			}
		}
	}
	else
	{
		shrink_wrap = false;
	}

	F32 radius;
	if (new_bin_radius)
	{
		if (alpha_wrap)
		{
			const LLVector3& bounds = getScale();
			radius = llmin(bounds.mV[0], bounds.mV[1], bounds.mV[2]) * 0.5f;
		}
		else if (shrink_wrap)
		{
			radius = mDrawable->getRadius() * 0.25f;
		}
		else
		{
			radius = llmax(1.f, mDrawable->getRadius(), size_factor);
		}
	}
	else if (alpha_wrap)
	{
		LLVector3 alpha_dist_factor = alpha_factor;
		const LLVector3& bounds = getScale();
		radius = llmin(bounds.mV[0], bounds.mV[1], bounds.mV[2]) * 0.5f;
		radius *= 1.f + mDrawable->mDistanceWRTCamera * alpha_dist_factor[1];
		radius += mDrawable->mDistanceWRTCamera * alpha_dist_factor[0];
	}
	else if (shrink_wrap)
	{
		const LLVector4a* extp = mDrawable->getSpatialExtents();
		LLVector4a rad;
		rad.setSub(extp[1], extp[0]);
		radius = rad.getLength3().getF32() * 0.5f;
	}
	else if (mDrawable->isStatic())
	{
		LLVector3 distance_factor = dist_factor;
		F32 szf = llmax(1.f, size_factor);
		radius = llmax(mDrawable->getRadius(), szf);
		radius = powf(radius, 1.f + szf / radius);
		radius *= 1.f + mDrawable->mDistanceWRTCamera * distance_factor[1];
		radius += mDrawable->mDistanceWRTCamera * distance_factor[0];
	}
	else if (mDrawable->getVObj()->isAttachment())
	{
		F32 attachment_size_factor = llmax(1.f, att_size_factor);
		radius = llmax(1.f, mDrawable->getRadius()) * attachment_size_factor;
	}
	else
	{
		LLVector3 distance_factor = dist_factor;
		radius = mDrawable->getRadius();
		radius *= 1.f + mDrawable->mDistanceWRTCamera * distance_factor[1];
		radius += mDrawable->mDistanceWRTCamera * distance_factor[0];
	}

	return llclamp(radius, 0.5f, 256.f);
}

const LLVector3 LLVOVolume::getPivotPositionAgent() const
{
	return mVolumeImpl ? mVolumeImpl->getPivotPosition()
					   : LLViewerObject::getPivotPositionAgent();
}

void LLVOVolume::onShift(const LLVector4a& shift_vector)
{
	if (mVolumeImpl)
	{
		mVolumeImpl->onShift(shift_vector);
	}
	updateRelativeXform();
}

const LLMatrix4& LLVOVolume::getWorldMatrix(LLXformMatrix* xform) const
{
	return mVolumeImpl ? mVolumeImpl->getWorldMatrix(xform)
					   : xform->getWorldMatrix();
}

//virtual
void LLVOVolume::markForUpdate(bool rebuild_all)
{
	if (mDrawable.notNull())
	{
		 shrinkWrap();
	}
	LLViewerObject::markForUpdate(rebuild_all);
	mVolumeChanged = true;
}

LLVector3 LLVOVolume::agentPositionToVolume(const LLVector3& pos) const
{
	LLVector3 ret = pos - getRenderPosition();
	ret = ret * ~getRenderRotation();
	if (!isVolumeGlobal())
	{
		LLVector3 obj_scale = getScale();
		LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX], 1.f / obj_scale.mV[VY],
							  1.f / obj_scale.mV[VZ]);
		ret.scaleVec(inv_obj_scale);
	}
	return ret;
}

LLVector3 LLVOVolume::agentDirectionToVolume(const LLVector3& dir) const
{
	LLVector3 ret = dir * ~getRenderRotation();

	LLVector3 obj_scale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	ret.scaleVec(obj_scale);

	return ret;
}

LLVector3 LLVOVolume::volumePositionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	if (!isVolumeGlobal())
	{
		LLVector3 obj_scale = getScale();
		ret.scaleVec(obj_scale);
	}

	ret *= getRenderRotation();
	ret += getRenderPosition();

	return ret;
}

LLVector3 LLVOVolume::volumeDirectionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	LLVector3 obj_scale = isVolumeGlobal() ? LLVector3(1.f, 1.f, 1.f)
										   : getScale();
	LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX], 1.f / obj_scale.mV[VY],
							1.f / obj_scale.mV[VZ]);
	ret.scaleVec(inv_obj_scale);
	ret = ret * getRenderRotation();

	return ret;
}


bool LLVOVolume::lineSegmentIntersect(const LLVector4a& start,
									  const LLVector4a& end,
									  S32 face,
									  bool pick_transparent,
									  bool pick_rigged,
									  S32* face_hitp,
									  LLVector4a* intersection,
									  LLVector2* tex_coord,
									  LLVector4a* normal,
									  LLVector4a* tangent)

{
	if (!mCanSelect || mDrawable.isNull() || mDrawable->isDead() ||
#if 0
		(gSelectMgr.mHideSelectedObjects && isSelected()) ||
#endif
		!gPipeline.hasRenderType(mDrawable->getRenderType()))
	{
		return false;
	}

	LLVolume* volp = getVolume();
	if (!volp)
	{
		return false;
	}

	bool transform = true;
	if (mDrawable->isState(LLDrawable::RIGGED))
	{
		LLVOAvatar* avatarp = getAvatar();
		if (!avatarp || avatarp->isDead())
		{
			// Cannot intersect a rigged attachment associated with a NULL or
			// dead avatar. HB
			llwarns << (avatarp ? "Dead" : "NULL")
					<< " avatar for intersected rigged volume." << llendl;
			clearRiggedVolume();
			return false;
		}
		if (pick_rigged || (avatarp->isSelf() && LLFloaterTools::isVisible()))
		{
			updateRiggedVolume(true, LLRiggedVolume::DO_NOT_UPDATE_FACES);
			volp = mRiggedVolume;
			transform = false;
		}
		else
		{
			// Cannot pick rigged attachments on other avatars or when not in
			// build mode
			return false;
		}
	}

	LLVector4a local_start = start;
	LLVector4a local_end = end;
	if (transform)
	{
		LLVector3 v_start(start.getF32ptr());
		LLVector3 v_end(end.getF32ptr());

		v_start = agentPositionToVolume(v_start);
		v_end = agentPositionToVolume(v_end);

		local_start.load3(v_start.mV);
		local_end.load3(v_end.mV);
	}

	LLVector4a p, n, tn;
	LLVector2 tc;
	if (intersection)
	{
		p = *intersection;
	}
	if (tex_coord)
	{
		tc = *tex_coord;
	}
	if (normal)
	{
		n = *normal;
	}
	if (tangent)
	{
		tn = *tangent;
	}

	S32 start_face, end_face;
	if (face == -1)
	{
		start_face = 0;
		end_face = volp->getNumVolumeFaces();
	}
	else
	{
		start_face = face;
		end_face = face + 1;
	}

	pick_transparent |= isHiglightedOrBeacon();

	bool ret = false;
	S32 face_hit = -1;

	bool special_cursor = specialHoverCursor();
	S32 num_faces = mDrawable->getNumFaces();
	for (S32 i = start_face; i < end_face; ++i)
	{
		if (!special_cursor && !pick_transparent && getTE(i) &&
			getTE(i)->isTransparent())
		{
			// Do not attempt to pick completely transparent faces unless
			// pick_transparent is true
			continue;
		}

		// This calculates the bounding box of the skinned mesh from scratch.
		// It is actually quite expensive, but not nearly as expensive as
		// building a full octree. rebuild_face_octrees = false because an
		// octree for this face will be built later only if needed for narrow
		// phase picking.
		updateRiggedVolume(true, i, false);

		face_hit = volp->lineSegmentIntersect(local_start, local_end, i, &p,
											  &tc, &n, &tn);
		if (face_hit < 0 || face_hit >= num_faces)
		{
			continue;
		}

		LLFace* face = mDrawable->getFace(face_hit);
		if (!face)
		{
			continue;
		}

		bool ignore_alpha = false;
		const LLTextureEntry* tep = face->getTextureEntry();
		if (tep)
		{
			LLMaterial* matp = tep->getMaterialParams();
			if (matp)
			{
				U8 mode = matp->getDiffuseAlphaMode();
				if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE ||
					mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE ||
					(mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK &&
					 matp->getAlphaMaskCutoff() == 0))
				{
					ignore_alpha = true;
				}
			}
		}

		if (ignore_alpha || pick_transparent || !face->getTexture() ||
			!face->getTexture()->hasGLTexture() ||
			face->getTexture()->getMask(face->surfaceToTexture(tc, p, n)))
		{
			local_end = p;
			if (face_hitp)
			{
				*face_hitp = face_hit;
			}

			if (intersection)
			{
				if (transform)
				{
					LLVector3 v_p(p.getF32ptr());
					// Must map back to agent space:
					intersection->load3(volumePositionToAgent(v_p).mV);
				}
				else
				{
					*intersection = p;
				}
			}

			if (normal)
			{
				if (transform)
				{
					LLVector3 v_n(n.getF32ptr());
					normal->load3(volumeDirectionToAgent(v_n).mV);
				}
				else
				{
					*normal = n;
				}
				(*normal).normalize3fast();
			}

			if (tangent)
			{
				if (transform)
				{
					LLVector3 v_tn(tn.getF32ptr());

					LLVector4a trans_tangent;
					trans_tangent.load3(volumeDirectionToAgent(v_tn).mV);

					LLVector4Logical mask;
					mask.clear();
					mask.setElement<3>();

					tangent->setSelectWithMask(mask, tn, trans_tangent);
				}
				else
				{
					*tangent = tn;
				}
					(*tangent).normalize3fast();
			}

			if (tex_coord)
			{
				*tex_coord = tc;
			}

			ret = true;
		}
	}

	return ret;
}

bool LLVOVolume::treatAsRigged()
{
	return isSelected() && mDrawable.notNull() &&
		   mDrawable->isState(LLDrawable::RIGGED) &&
		   (isAttachment() || isAnimatedObject());
}

void LLVOVolume::clearRiggedVolume()
{
	if (mRiggedVolume.notNull())
	{
		mRiggedVolume = NULL;
		updateRelativeXform();
	}
}

// Updates mRiggedVolume to match current animation frame of avatar. Also
// updates position/size in octree.
void LLVOVolume::updateRiggedVolume(bool force_treat_as_rigged, S32 face_index,
									bool rebuild_face_octrees)
{
	if (isDead())
	{
		return;
	}

	if (!force_treat_as_rigged && !treatAsRigged())
	{
		clearRiggedVolume();
		return;
	}

	LLVolume* volp = getVolume();
	if (!volp) return;

	const LLMeshSkinInfo* skinp = getSkinInfo();
	if (!skinp)
	{
		clearRiggedVolume();
		return;
	}

	LLVOAvatar* avatarp = getAvatar();
	if (!avatarp || avatarp->isDead())
	{
		clearRiggedVolume();
		return;
	}

	if (!mRiggedVolume)
	{
		LLVolumeParams p;
		mRiggedVolume = new LLRiggedVolume(p);
		updateRelativeXform();
	}

	mRiggedVolume->update(skinp, avatarp, volp, face_index,
						  rebuild_face_octrees);
}

void LLRiggedVolume::update(const LLMeshSkinInfo* skinp, LLVOAvatar* avatarp,
							const LLVolume* volp, S32 face_index,
							bool rebuild_face_octrees)
{
	LL_FAST_TIMER(FTM_UPDATE_RIGGED_VOLUME);

	bool copy = volp->getNumVolumeFaces() != getNumVolumeFaces();
	if (!copy)
	{
		for (S32 i = 0, count = volp->getNumVolumeFaces(); i < count; ++i)
		{
			const LLVolumeFace& src_face = volp->getVolumeFace(i);
			const LLVolumeFace& dst_face = getVolumeFace(i);
			if (src_face.mNumIndices != dst_face.mNumIndices ||
				src_face.mNumVertices != dst_face.mNumVertices)
			{
				copy = true;
				break;
			}
		}
	}
	if (copy)
	{
		copyVolumeFaces(volp);
	}
	else if (!avatarp || avatarp->isDead() ||
			 avatarp->getMotionController().isReallyPaused())
	{
		return;
	}

	S32 face_begin, face_end;
	if (face_index == DO_NOT_UPDATE_FACES)
	{
		face_begin = face_end = 0;
	}
	else if (face_index == UPDATE_ALL_FACES)
	{
		face_begin = 0;
		face_end = volp->getNumVolumeFaces();
	}
	else
	{
		face_begin = face_index;
		face_end = face_begin + 1;
	}

	// Build matrix palette
	U32 count = 0;
	const LLMatrix4a* matp = avatarp->getRiggedMatrix4a(skinp, count);

	LLVector4a t, dst, size;
	LLMatrix4a final_mat, bind_shape_matrix;
	bind_shape_matrix.loadu(skinp->mBindShapeMatrix);
	for (S32 i = face_begin; i < face_end; ++i)
	{
		const LLVolumeFace& vol_face = volp->getVolumeFace(i);

		LLVolumeFace& dst_face = mVolumeFaces[i];

		LLVector4a* weight = vol_face.mWeights;
		if (!weight) continue;

		LLSkinningUtil::checkSkinWeights(weight, dst_face.mNumVertices, skinp);

		LLVector4a* pos = dst_face.mPositions;
		if (pos && dst_face.mExtents)
		{
			for (S32 j = 0; j < dst_face.mNumVertices; ++j)
			{
				LLSkinningUtil::getPerVertexSkinMatrix(weight[j], matp,
													   final_mat);
				LLVector4a& v = vol_face.mPositions[j];
				bind_shape_matrix.affineTransform(v, t);
				final_mat.affineTransform(t, dst);
				pos[j] = dst;
			}

			// Update bounding box
			LLVector4a& min = dst_face.mExtents[0];
			LLVector4a& max = dst_face.mExtents[1];

			min = pos[0];
			max = pos[1];

			for (S32 j = 1; j < dst_face.mNumVertices; ++j)
			{
				min.setMin(min, pos[j]);
				max.setMax(max, pos[j]);
			}

			dst_face.mCenter->setAdd(dst_face.mExtents[0],
									 dst_face.mExtents[1]);
			dst_face.mCenter->mul(0.5f);

			if (rebuild_face_octrees)
			{
				LL_FAST_TIMER(FTM_RIGGED_OCTREE);
				dst_face.destroyOctree();
				dst_face.createOctree();
			}
		}
	}
}

U32 LLVOVolume::getPartitionType() const
{
	if (isHUDAttachment())
	{
		return LLViewerRegion::PARTITION_HUD;
	}
	if (isAnimatedObject() && getPuppetAvatar())
	{
		return LLViewerRegion::PARTITION_PUPPET;
	}
	if (isAttachment())
	{
		return LLViewerRegion::PARTITION_AVATAR;
	}
	return LLViewerRegion::PARTITION_VOLUME;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumePartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLVolumePartition::LLVolumePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(LLVOVolume::VERTEX_DATA_MASK, true, regionp),
	LLVolumeGeometryManager()
{
	mLODPeriod = 32;
	mDepthMask = false;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_VOLUME;
	mSlopRatio = 0.25f;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumeBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLVolumeBridge::LLVolumeBridge(LLDrawable* drawablep, LLViewerRegion* regionp)
:	LLSpatialBridge(drawablep, true, LLVOVolume::VERTEX_DATA_MASK, regionp),
	LLVolumeGeometryManager()
{
	mDepthMask = false;
	mLODPeriod = 32;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_BRIDGE;
	mSlopRatio = 0.25f;
}

///////////////////////////////////////////////////////////////////////////////
// LLAvatarBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLAvatarBridge::LLAvatarBridge(LLDrawable* drawablep,
							   LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_AVATAR;
	mPartitionType = LLViewerRegion::PARTITION_AVATAR;
}

///////////////////////////////////////////////////////////////////////////////
// LLPuppetBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLPuppetBridge::LLPuppetBridge(LLDrawable* drawablep,
							   LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_PUPPET;
	mPartitionType = LLViewerRegion::PARTITION_PUPPET;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumeGeometryManager class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLVolumeGeometryManager::LLVolumeGeometryManager()
:	LLGeometryManager()
{
	if (sInstanceCount == 0)
	{
		allocateFaces(MAX_FACE_COUNT);
	}

	++sInstanceCount;
}

LLVolumeGeometryManager::~LLVolumeGeometryManager()
{
	llassert(sInstanceCount > 0);
	--sInstanceCount;

	if (sInstanceCount <= 0)
	{
		freeFaces();
		sInstanceCount = 0;
	}
}

void LLVolumeGeometryManager::allocateFaces(U32 max_face_count)
{
	size_t bytes = max_face_count * sizeof(LLFace*);
	for (U32 i = 0; i < 2; ++i)
	{
		sFullbrightFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sBumpFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sSimpleFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sNormFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sSpecFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sNormSpecFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sPbrFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
		sAlphaFaces[i] = (LLFace**)ll_aligned_malloc(bytes, 64);
	}
}

void LLVolumeGeometryManager::freeFaces()
{
	for (U32 i = 0; i < 2; ++i)
	{
		ll_aligned_free(sFullbrightFaces[i]);
		ll_aligned_free(sBumpFaces[i]);
		ll_aligned_free(sSimpleFaces[i]);
		ll_aligned_free(sNormFaces[i]);
		ll_aligned_free(sSpecFaces[i]);
		ll_aligned_free(sNormSpecFaces[i]);
		ll_aligned_free(sPbrFaces[i]);
		ll_aligned_free(sAlphaFaces[i]);

		sFullbrightFaces[i] = NULL;
		sBumpFaces[i] = NULL;
		sSimpleFaces[i] = NULL;
		sNormFaces[i] = NULL;
		sSpecFaces[i] = NULL;
		sNormSpecFaces[i] = NULL;
		sPbrFaces[i] = NULL;
		sAlphaFaces[i] = NULL;
	}
}

// Helper function for opacity test during rendering
static bool opaque_face(const LLFace* facep, const LLTextureEntry* tep)
{
	if (facep->isState(LLFace::USE_FACE_COLOR))
	{
		return facep->getFaceColor().mV[3] >= 0.999f;
	}
	return tep->isOpaque();
}

void LLVolumeGeometryManager::registerFace(LLSpatialGroup* groupp,
										   LLFace* facep, U32 type)
{
	LL_FAST_TIMER(FTM_REGISTER_FACE);

	if (!facep || !groupp) return;	// Paranoia

	if (facep->getViewerObject()->isSelected() &&
//MK
		(!gRLenabled || !gRLInterface.mContainsEdit) &&
//mk
		gSelectMgr.mHideSelectedObjects)
	{
		return;
	}

	const LLTextureEntry* tep = facep->getTextureEntry();
	if (!tep)
	{
		llwarns_sparse << "NULL texture entry pointer. Aborting." << llendl;
		return;
	}

	bool rigged = facep->isState(LLFace::RIGGED);

	// Add face to drawmap
	LLSpatialGroup::drawmap_elem_t& draw_vec =
		groupp->mDrawMap[rigged ? type + 1 : type];

	bool fullbright = type == LLRenderPass::PASS_FULLBRIGHT ||
					  type == LLRenderPass::PASS_INVISIBLE ||
					  type == LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK ||
					  (type == LLRenderPass::PASS_ALPHA &&
					   facep->isState(LLFace::FULLBRIGHT)) ||
					  tep->getFullbright();

	if (!fullbright && type != LLRenderPass::PASS_GLOW &&
		!facep->getVertexBuffer()->hasDataType(LLVertexBuffer::TYPE_NORMAL))
	{
		llwarns_sparse << "Non fullbright face has no normals !" << llendl;
		return;
	}

	F32 vsize = facep->getVirtualSize();	// *TODO: adjust by texture scale ?

	const LLMatrix4* tex_mat = NULL;
	if (vsize >= MIN_TEX_ANIM_SIZE && facep->isState(LLFace::TEXTURE_ANIM))
	{
		tex_mat = facep->mTextureMatrix;
	}

	LLDrawable* drawable = facep->getDrawable();
	if (!drawable) return;	// Paranoia

	const LLMatrix4* model_mat = NULL;
	if (!rigged)	// Rigged meshes ignore their model matrix
	{
		if (drawable->isState(LLDrawable::ANIMATED_CHILD))
		{
			model_mat = &drawable->getWorldMatrix();
		}
		else if (drawable->isActive())
		{
			model_mat = &drawable->getRenderMatrix();
		}
		else if (drawable->getRegion())
		{
			model_mat = &(drawable->getRegion()->mRenderMatrix);
		}
#if 1
		if (model_mat && model_mat->isIdentity())
		{
			model_mat = NULL;
		}
#endif
	}

	U8 bump = type == LLRenderPass::PASS_BUMP ||
			  type == LLRenderPass::PASS_POST_BUMP ? tep->getBumpmap() : 0;

	U8 shiny = tep->getShiny();

	U8 index = facep->getTextureIndex();

	LLMaterial* matp = tep->getMaterialParams().get();

	LLGLTFMaterial* rmatp = tep->getGLTFRenderMaterial();
	LLFetchedGLTFMaterial* gltfp = rmatp ? rmatp->asFetched() : NULL;
	// *HACK: when we have a GLTF material and are not rendering in PBR mode,
	// and the face does not have any fallback diffuse texture set, try and
	// use the base color texture for the diffuse channel.
	// Note: we use the USE_FACE_COLOR state as a marker for overridden diffuse
	// texture; this is OK, since the only other use for this state is with sky
	// and classic cloud faces, which do not bear a GLTF material. HB
	static LLCachedControl<U32> use_basecolor(gSavedSettings,
											  "RenderUseBasecolorAsDiffuse");
	// Do NOT touch the diffuse texture when it is bearing a media texture,
	// since it then itself makes use of switchTexture() on the diffuse
	// channel, which would cause conflicts. Also, when we have a legacy
	// material, we should not either override its diffuse texture (considering
	// that in this case the creator did provide an adequate legacy material in
	// excesss of the PBR material). HB
	bool may_touch_diffuse = gltfp && !matp && !gUsePBRShaders &&
							 !facep->hasMedia();
	const LLUUID& basecolor_id = may_touch_diffuse ? gltfp->getBaseColorId()
												   : LLUUID::null;
	bool got_base_color_tex = basecolor_id.notNull();
	if (may_touch_diffuse && use_basecolor &&
		(got_base_color_tex || use_basecolor > 2) &&
		// Note: we do not apply our hack while editing this face: we want to
		// still be able to see and edit the diffuse texture on GLTF-enabled
		// faces. HB
		(!tep->isSelected() || !LLFloaterTools::isVisible()))
	{
		if (got_base_color_tex && (use_basecolor > 1 || tep->isDefault()))
		{
			// Set to base color texture and color
			facep->switchDiffuseTex(basecolor_id);
			facep->setFaceColor(gltfp->mBaseColor);
		}
		else if (use_basecolor > 2)
		{
			// Set texture to blank and color to base color
			facep->switchDiffuseTex(IMG_BLANK);
			facep->setFaceColor(gltfp->mBaseColor);
		}
		else if (facep->isState(LLFace::USE_FACE_COLOR))
		{
			// Reset to diffuse texture and color
			facep->switchDiffuseTex(tep->getID());
			facep->unsetFaceColor();
		}
	}
	else if (may_touch_diffuse && facep->isState(LLFace::USE_FACE_COLOR))
	{
		// Reset to diffuse texture and color
		facep->switchDiffuseTex(tep->getID());
		facep->unsetFaceColor();
	}
	// Render without the GLTF material when we are not in PBR mode. HB
	if (!gUsePBRShaders)
	{
		gltfp = NULL;
	}

	LLViewerTexture* texp = facep->getTexture();
//MK
	// If @camtexture is set, do not show any texture in world (but show
	// attachments normally)
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		gRLInterface.mCamTexturesCustom &&
		!facep->getViewerObject()->isAttachment())
	{
		texp = gRLInterface.mCamTexturesCustom;
	}
//mk

	LLUUID mat_id;
	if (gltfp)
	{
		mat_id = gltfp->getHash();
		if (!facep->hasMedia())
		{
			// No media texture, face texture will be unused
			texp = NULL;
		}
		// Do not use any legacy material when we do have a PBR material to
		// render with. HB
		matp = NULL;
	}
	else if (matp)
	{
#if 0	// No need to slow down rendering with a live hashing of each
		// material !  HB
		mat_id = matp->getHash();
#else	// Instead, just copy the material Id into mat_id. HB
		mat_id = tep->getMaterialID().asUUID();
#endif
	}

	U32 shader_mask = 0xFFFFFFFF; // No shader
	if (matp)
	{
		bool is_alpha = facep->getPoolType() == LLDrawPool::POOL_ALPHA ||
						!opaque_face(facep, tep);
		if (type == LLRenderPass::PASS_ALPHA)
		{
			shader_mask =
				matp->getShaderMask(LLMaterial::DIFFUSE_ALPHA_MODE_BLEND,
									is_alpha);
		}
		else
		{
			shader_mask =
				matp->getShaderMask(LLMaterial::DIFFUSE_ALPHA_MODE_DEFAULT,
									is_alpha);
		}
	}

	S32 idx = draw_vec.size() - 1;
	LLDrawInfo* dinfop = idx >= 0 ? draw_vec[idx].get() : NULL;
	bool batchable = false;
	if (dinfop && index < FACE_DO_NOT_BATCH_TEXTURES &&
		facep->canBatchTexture())
	{
		if (index < dinfop->mTextureList.size())
		{
			if (dinfop->mTextureList[index].isNull())
			{
				batchable = true;
				dinfop->mTextureList[index] = texp;
				dinfop->mTextureListVSize[index] = vsize;
			}
			else if (dinfop->mTextureList[index] == texp)
			{
				// This face's texture index can be used with this batch
				batchable = true;
				if (dinfop->mTextureListVSize[index] < vsize)
				{
					dinfop->mTextureListVSize[index] = vsize;
				}
			}
		}
		else
		{
			// Texture list can be expanded to fit this texture index
			batchable = true;
		}
	}

	if (dinfop &&
		dinfop->mVertexBuffer == facep->getVertexBuffer() &&
		dinfop->mEnd == facep->getGeomIndex() - 1 &&
		(batchable || dinfop->mTexture == texp) &&
#if LL_DARWIN
		(S32)(dinfop->mEnd - dinfop->mStart +
			facep->getGeomCount()) <= gGLManager.mGLMaxVertexRange &&
		(S32)(dinfop->mCount +
			facep->getIndicesCount()) <= gGLManager.mGLMaxIndexRange &&
#endif
		dinfop->mMaterialID == mat_id &&
		dinfop->mFullbright == fullbright &&
		dinfop->mBump == bump &&
		// Need to break batches when a material is shared, but legacy shiny is
		// different
		(!matp || dinfop->mShiny == shiny) &&
		dinfop->mTextureMatrix == tex_mat &&
		dinfop->mModelMatrix == model_mat &&
		dinfop->mShaderMask == shader_mask &&
		dinfop->mAvatar == facep->mAvatar &&
		dinfop->getSkinHash() == facep->getSkinHash())
	{
		dinfop->mCount += facep->getIndicesCount();
		dinfop->mEnd += facep->getGeomCount();
		dinfop->mVSize = llmax(dinfop->mVSize, vsize);

		if (index < FACE_DO_NOT_BATCH_TEXTURES &&
			index >= dinfop->mTextureList.size())
		{
			dinfop->mTextureList.resize(index + 1);
			dinfop->mTextureListVSize.resize(index + 1);
			dinfop->mTextureList[index] = texp;
			dinfop->mTextureListVSize[index] = vsize;
		}
		dinfop->validate();
		update_min_max(dinfop->mExtents[0], dinfop->mExtents[1],
					   facep->mExtents[0]);
		update_min_max(dinfop->mExtents[0], dinfop->mExtents[1],
					   facep->mExtents[1]);
	}
	else
	{
		U32 start = facep->getGeomIndex();
		U32 end = start + facep->getGeomCount() - 1;
		U32 offset = facep->getIndicesStart();
		U32 count = facep->getIndicesCount();
		LLPointer<LLDrawInfo> draw_info =
			new LLDrawInfo(start, end, count, offset, texp,
						   facep->getVertexBuffer(), fullbright, bump);
		draw_vec.push_back(draw_info);
		draw_info->mVSize = vsize;
		draw_info->mTextureMatrix = tex_mat;
		draw_info->mModelMatrix = model_mat;
		draw_info->mBump = bump;
		draw_info->mShiny = shiny;

		static const F32 alpha[4] = { 0.f, 0.25f, 0.5f, 0.75f };
		F32 spec = alpha[shiny & TEM_SHINY_MASK];
		draw_info->mSpecColor.set(spec, spec, spec, spec);
		draw_info->mEnvIntensity = spec;
		draw_info->mSpecularMap = NULL;
		draw_info->mMaterial = matp;
		draw_info->mGLTFMaterial = gltfp;
		draw_info->mShaderMask = shader_mask;
		draw_info->mAvatar = facep->mAvatar;
		draw_info->mSkinInfo = facep->mSkinInfo;

		if (gltfp)
		{
			// Just remember the material Id; render pools will reference the
			// GLTF material.
			draw_info->mMaterialID = mat_id;
		}
		else if (matp)
		{
			// We have a material. Update our draw info accordingly.
			draw_info->mMaterialID = mat_id;
			S32 te_offset = facep->getTEOffset();
			if (matp->getSpecularID().notNull())
			{
				const LLColor4U& spec_col = matp->getSpecularLightColor();
				F32 alpha = (F32)matp->getSpecularLightExponent() * ONE255TH;
				draw_info->mSpecColor.set(spec_col.mV[0] * ONE255TH,
										  spec_col.mV[1] * ONE255TH,
										  spec_col.mV[2] * ONE255TH, alpha);

				draw_info->mEnvIntensity = matp->getEnvironmentIntensity() *
										   ONE255TH;
				draw_info->mSpecularMap =
					facep->getViewerObject()->getTESpecularMap(te_offset);
			}
			draw_info->mAlphaMaskCutoff = matp->getAlphaMaskCutoff() *
										  ONE255TH;
			draw_info->mDiffuseAlphaMode = matp->getDiffuseAlphaMode();
			draw_info->mNormalMap =
				facep->getViewerObject()->getTENormalMap(te_offset);
		}
		else if (type == LLRenderPass::PASS_GRASS)
		{
			draw_info->mAlphaMaskCutoff = 0.5f;
		}
		else
		{
			draw_info->mAlphaMaskCutoff = 0.33f;
		}
#if 0	// Always populate the draw_info
		if (type == LLRenderPass::PASS_ALPHA)
#endif
		{
			facep->setDrawInfo(draw_info);	// For alpha sorting
		}
		draw_info->mExtents[0] = facep->mExtents[0];
		draw_info->mExtents[1] = facep->mExtents[1];

		if (index < FACE_DO_NOT_BATCH_TEXTURES)
		{
			// Initialize texture list for texture batching
			draw_info->mTextureList.resize(index + 1);
			draw_info->mTextureListVSize.resize(index + 1);
			draw_info->mTextureList[index] = texp;
			draw_info->mTextureListVSize[index] = vsize;
		}
		draw_info->validate();
	}
}

// Adds a face pointer to a list of face pointers without going over MAX_COUNT
template<typename T>
static inline void add_face(T*** list, U32* count, T* face)
{
	if (face->isState(LLFace::RIGGED))
	{
		if (count[1] < MAX_FACE_COUNT)
		{
			list[1][count[1]++] = face;
		}
	}
	else if (count[0] < MAX_FACE_COUNT)
	{
		list[0][count[0]++] = face;
	}
}

// Returns the index in linkset for a given object (0 for root prim)
static U32 get_linkset_index(const LLVOVolume* vobjp)
{
	if (vobjp->isRootEdit())
	{
		return 0;
	}

	U32 idx = 1;

	const LLViewerObject* rootp = vobjp->getRootEdit();
	if (!rootp)	// Paranoia
	{
		return idx;
	}

	LLViewerObject::const_child_list_t& child_list = rootp->getChildren();
	for (LLViewerObject::const_child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter, ++idx)
	{
		if (iter->get() == vobjp)
		{
			break;
		}
	}

	return idx;
}

// Helper function for transparency test during rendering
static bool transparent_face(const LLGLTFMaterial* gltfp, const LLFace* facep,
						 	 const LLTextureEntry* tep)
{
	if (tep->hasGlow())
	{
		return false;
	}
	if (gltfp)
	{
		return gltfp->mBaseColor.mV[3] < 0.001f;
	}
	if (facep->isState(LLFace::USE_FACE_COLOR))
	{
		return facep->getFaceColor().mV[3] < 0.001f;
	}
	return tep->isTransparent();
}

//virtual
void LLVolumeGeometryManager::rebuildGeom(LLSpatialGroup* groupp)
{
	LL_FAST_TIMER(FTM_REBUILD_VBO);

	if (groupp->isDead())
	{
		return;
	}

	if (groupp->changeLOD())
	{
		groupp->mLastUpdateDistance = groupp->mDistance;
	}

	groupp->mLastUpdateViewAngle = groupp->mViewAngle;

	if (!groupp->hasState(LLSpatialGroup::GEOM_DIRTY |
						  LLSpatialGroup::ALPHA_DIRTY))
	{
		if (groupp->hasState(LLSpatialGroup::MESH_DIRTY))
		{
			rebuildMesh(groupp);
		}
		return;
	}

	groupp->mBuilt = 1.f;

	LLVOAvatar* voavp = NULL;
	LLVOAvatar* voattachavp = NULL;
	LLSpatialPartition* partp = groupp->getSpatialPartition();
	if (!partp) return;	// Paranoia
	LLSpatialBridge* bridge = partp->asBridge();
	LLVOVolume* vovolp = NULL;
	if (bridge)
	{
		LLViewerObject* vobjp =
			bridge->mDrawable ? bridge->mDrawable->getVObj().get() : NULL;
		if (vobjp)
		{
			if (bridge->mAvatar.isNull())
			{
				bridge->mAvatar = vobjp->getAvatar();
			}
			voattachavp = vobjp->getAvatarAncestor();
			vovolp = vobjp->asVolume();
		}
		voavp = bridge->mAvatar;
	}
	if (voattachavp)
	{
		voattachavp->subtractAttachmentBytes(groupp->mGeometryBytes);
		voattachavp->subtractAttachmentArea(groupp->mSurfaceArea);
	}
	if (voavp && voavp != voattachavp)
	{
		voavp->subtractAttachmentBytes(groupp->mGeometryBytes);
		voavp->subtractAttachmentArea(groupp->mSurfaceArea);
	}
	if (vovolp)
	{
		vovolp->updateVisualComplexity();
	}

	groupp->mGeometryBytes = 0;
	groupp->mSurfaceArea = 0.f;

	// Cache object box size since it might be used for determining visibility
	const LLVector4a* bounds = groupp->getObjectBounds();
	groupp->mObjectBoxSize = llmax(bounds[1].getLength3().getF32(), 10.f);

	groupp->clearDrawMap();

	U32 fullbright_count[2] = { 0 };
	U32 bump_count[2] = { 0 };
	U32 simple_count[2] = { 0 };
	U32 alpha_count[2] = { 0 };
	U32 norm_count[2] = { 0 };
	U32 spec_count[2] = { 0 };
	U32 normspec_count[2] = { 0 };
	U32 pbr_count[2] = { 0 };

	U32 vertex_size =
		(U32)LLVertexBuffer::calcVertexSize(partp->mVertexDataMask);
	U32 max_vertices = LLVOVolume::sRenderMaxVBOSize * 1024 / vertex_size;
	max_vertices = llmin(max_vertices, 65535U);

	U32 cur_total = 0;
	static LLCachedControl<U32> render_max_node_size(gSavedSettings,
													 "RenderMaxNodeSize");
	U32 max_total = render_max_node_size * 1024U / vertex_size;
	static LLCachedControl<F32> mesh_boost(gSavedSettings,
										   "MeshLODBoostFactor");
	F32 mesh_geom_factor = 1.f;
	if (mesh_boost > 1.f)
	{
		mesh_geom_factor =
			1.f / llmin((F32)mesh_boost * mesh_boost * mesh_boost, 16.f);
	}

	bool use_wl_shaders = gPipeline.canUseWindLightShaders();

	bool debugging_alpha = LLDrawPoolAlpha::sShowDebugAlpha;
	bool emissive = false;

	{
		LL_FAST_TIMER(FTM_REBUILD_VOLUME_FACE_LIST);

		static LLCachedControl<U32> use_basecolor(gSavedSettings,
												  "RenderUseBasecolorAsDiffuse");
		// Get all the faces into a list
		for (LLSpatialGroup::element_iter it = groupp->getDataBegin();
			 it != groupp->getDataEnd(); ++it)
		{
			LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
			if (!drawablep || drawablep->isDead() ||
				drawablep->isState(LLDrawable::FORCE_INVISIBLE))
			{
				continue;
			}

			LLVOVolume* vobjp = drawablep->getVOVolume();
			if (!vobjp || vobjp->isDead())
			{
				continue;
			}

			LLVolume* volp = vobjp->getVolume();
			if (!volp)
			{
				continue;
			}

			if (gUsePBRShaders || use_basecolor)
			{
				// *HACK: brute force this check every time a drawable gets
				// rebuilt.
				for (S32 i = 0, count = drawablep->getNumFaces(); i < count;
					 ++i)
				{
					vobjp->updateTEMaterialTextures(i);
				}
				// Apply any pending material overrides
				gGLTFMaterialList.applyQueuedOverrides(vobjp);
			}

			F32 geom_count_factor = 1.f;
			bool is_mesh = vobjp->isMesh();
			if (is_mesh)
			{
				if (!gMeshRepo.meshRezEnabled() || !volp->isMeshAssetLoaded())
				{
					continue;
				}
				geom_count_factor = mesh_geom_factor;
			}

			vobjp->updatePuppetAvatar();

			const LLVector3& scale = vobjp->getScale();
			groupp->mSurfaceArea += volp->getSurfaceArea() *
									llmax(scale.mV[0], scale.mV[1],
										  scale.mV[2]);

			{
				LL_FAST_TIMER(FTM_VOLUME_TEXTURES);
				vobjp->updateTextureVirtualSize(true);
			}
			vobjp->preRebuild();

			drawablep->clearState(LLDrawable::HAS_ALPHA);

			// *TODO: clean up the mess with all the avatar pointers. HB
			LLVOAvatarPuppet* pup = vobjp->getPuppetAvatar();
			voavp = pup;
			bool animated = vobjp->isAnimatedObject();
			LLVOAvatar* vobj_av = vobjp->getAvatar();
			LLVOAvatar* avatarp = NULL;
			const LLMeshSkinInfo* skin_infop = is_mesh ? vobjp->getSkinInfo()
													   : NULL;
			if (skin_infop)
			{
				avatarp = animated ? voavp : vobj_av;
			}
			if (avatarp)
			{
				avatarp->addAttachmentOverridesForObject(vobjp, NULL, false);
			}

			U32 linkset_index = get_linkset_index(vobjp);

			// Standard rigged attachment (non animated mesh)
			bool rigged = !animated && skin_infop && vobjp->isAttachment();
			// Animated objects. Have to check for isRiggedMesh() to exclude
			// static objects in animated object linksets.
			rigged |= animated && vobjp->isRiggedMesh() && pup && pup->mPlaying;

			bool is_rigged = false;

			// For each face
			for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
			{
				LLFace* facep = drawablep->getFace(i);
				if (!facep)
				{
					continue;
				}

				const LLTextureEntry* tep = facep->getTextureEntry();
				if (!tep)
				{
					continue;
				}

				LLGLTFMaterial* gltfp = NULL;
				if (gUsePBRShaders)
				{
					gltfp = tep->getGLTFRenderMaterial();
				}

				// Order by linkset index first and face index second
				facep->setDrawOrderIndex(linkset_index * 100 + i);

				// ALWAYS null out vertex buffer on rebuild: if the face lands
				// in a render batch, it will recover its vertex buffer
				// reference from the spatial group
				facep->setVertexBuffer(NULL);

				// Sum up face verts and indices
				drawablep->updateFaceSize(i);

				if (rigged)
				{
					if (!facep->isState(LLFace::RIGGED))
					{
						// Completely reset vertex buffer
						facep->clearVertexBuffer();
					}
					facep->setState(LLFace::RIGGED);
					facep->mSkinInfo = (LLMeshSkinInfo*)skin_infop;
					facep->mAvatar = avatarp;
					is_rigged = true;
				}
				else if (facep->isState(LLFace::RIGGED))
				{
					// Face is not rigged but used to be, remove from rigged
					// face pool
					facep->clearState(LLFace::RIGGED);
					facep->mSkinInfo = NULL;
					facep->mAvatar = NULL;
				}

				if (facep->getIndicesCount() <= 0 ||
					facep->getGeomCount() <= 0)
				{
					facep->clearVertexBuffer();
					continue;
				}

				if (cur_total > max_total)
				{
					llwarns_sparse << "Skipping rendering due to excessive node size."
								   << llendl;
					facep->clearVertexBuffer();
					continue;
				}

				if (facep->hasGeometry() &&
					// *FIXME: getPixelArea() is sometimes incorrect for rigged
					// meshes (thus the test for 'rigged', which is a hack).
					(rigged || facep->getPixelArea() > FORCE_CULL_AREA))
				{
					cur_total += facep->getGeomCount() * geom_count_factor;

					LLViewerTexture* texp = facep->getTexture();

					if (tep->hasGlow())
					{
						emissive = true;
					}

					if (facep->isState(LLFace::TEXTURE_ANIM) &&
						!vobjp->mTexAnimMode)
					{
						facep->clearState(LLFace::TEXTURE_ANIM);
					}

					bool force_simple =
						facep->getPixelArea() < FORCE_SIMPLE_RENDER_AREA;
					U32 type = gPipeline.getPoolTypeFromTE(tep, texp);
					if (gltfp &&
						gltfp->mAlphaMode != LLGLTFMaterial::ALPHA_MODE_BLEND)
					{
						type = LLDrawPool::POOL_MAT_PBR;
					}
					else if (type != LLDrawPool::POOL_ALPHA && force_simple)
					{
						type = LLDrawPool::POOL_SIMPLE;
					}
					facep->setPoolType(type);

					if (!gltfp && vobjp->isHUDAttachment())
					{
						facep->setState(LLFace::FULLBRIGHT);
					}

					if (vobjp->mTextureAnimp && vobjp->mTexAnimMode)
					{
						if (vobjp->mTextureAnimp->mFace <= -1)
						{
							for (S32 face = 0, count2 = vobjp->getNumTEs();
								 face < count2; ++face)
							{
								LLFace* facep2 = drawablep->getFace(face);
								if (facep2)
								{
									facep2->setState(LLFace::TEXTURE_ANIM);
								}
							}
						}
						else if (vobjp->mTextureAnimp->mFace <
									vobjp->getNumTEs())
						{
							LLFace* facep2 =
								drawablep->getFace(vobjp->mTextureAnimp->mFace);
							if (facep2)
							{
								facep2->setState(LLFace::TEXTURE_ANIM);
							}
						}
					}

					if (type == LLDrawPool::POOL_ALPHA)
					{
						if (facep->canRenderAsMask())
						{
							// Can be treated as alpha mask
							add_face(sSimpleFaces, simple_count, facep);
						}
						else
						{
							bool transparent = transparent_face(gltfp, facep,
																tep);
							if (!transparent)
							{
								// Only treat as alpha in the pipeline if not
								// fully transparent
								drawablep->setState(LLDrawable::HAS_ALPHA);
								add_face(sAlphaFaces, alpha_count, facep);
							}
							else if (debugging_alpha)
							{
								// When debugging alpha, also add fully
								// transparent faces.
								add_face(sAlphaFaces, alpha_count, facep);
							}
						}
					}
					else
					{
						if (drawablep->isState(LLDrawable::REBUILD_VOLUME))
						{
							facep->mLastUpdateTime = gFrameTimeSeconds;
						}

						if (use_wl_shaders)
						{
							LLMaterial* matp = NULL;
							if (LLPipeline::sRenderDeferred && !gltfp &&
								tep->getMaterialID().notNull())
							{
								matp = tep->getMaterialParams().get();
							}
							if (matp)
							{
								// If face got an emboss bump map, it needs
								// tangents.
								U8 bump = tep->getBumpmap();
								if ((bump && bump < 18) ||
									matp->getNormalID().notNull())
								{
									if (matp->getSpecularID().notNull())
									{
										add_face(sNormSpecFaces, normspec_count,
												 facep);
									}
									// Has normal map: needs texcoord1 and
									// tangent
									else
									{
										add_face(sNormFaces, norm_count,
												 facep);
									}
								}
								else if (matp->getSpecularID().notNull())
								{
									// Has specular map but no normal map,
									// needs texcoord2
									add_face(sSpecFaces, spec_count, facep);
								}
								// Has neither specular map nor normal map,
								// only needs texcoord0
								else
								{
									add_face(sSimpleFaces, simple_count,
											 facep);
								}
							}
							else if (gltfp)
							{
								add_face(sPbrFaces, pbr_count, facep);
							}
							else if (tep->getBumpmap())
							{
								// Needs normal + tangent
								add_face(sBumpFaces, bump_count, facep);
							}
							else if (tep->getShiny() || !tep->getFullbright())
							{
								// Needs normal
								add_face(sSimpleFaces, simple_count, facep);
							}
							else
							{
								// Does not need normal
								facep->setState(LLFace::FULLBRIGHT);
								add_face(sFullbrightFaces, fullbright_count,
										 facep);
							}
						}
						else if (tep->getBumpmap())
						{
							// Needs normal + tangent
							add_face(sBumpFaces, bump_count, facep);
						}
						else if (tep->getShiny() || !tep->getFullbright())
						{
							// Needs normal
							 add_face(sSimpleFaces, simple_count, facep);
						}
						else
						{
							// Does not need normal
							facep->setState(LLFace::FULLBRIGHT);
							add_face(sFullbrightFaces, fullbright_count,
									 facep);
						}
					}
				}
				else
				{
					// Face has no renderable geometry
					facep->clearVertexBuffer();
				}
			}

			if (is_rigged)
			{
				if (!drawablep->isState(LLDrawable::RIGGED))
				{
					drawablep->setState(LLDrawable::RIGGED);
					LLDrawable* rootp = drawablep->getRoot();
					if (rootp != drawablep)
					{
						rootp->setState(LLDrawable::RIGGED_CHILD);
					}
					// First time this is drawable is being marked as rigged,
					// do another LoD update to use avatar bounding box
					vobjp->updateLOD();
				}
			}
			else
			{
				drawablep->clearState(LLDrawable::RIGGED);
				vobjp->updateRiggedVolume(false);
			}
		}
	}

	// NOTE: MAP_TEXTURE_INDEX is part of BASE_MASK since it was always added
	// anyway as in 'extra_mask' to all masks in LL's original code. HB
	constexpr U32 BASE_MASK = LLVertexBuffer::MAP_TEXTURE_INDEX |
							  LLVertexBuffer::MAP_TEXCOORD0 |
							  LLVertexBuffer::MAP_VERTEX |
							  LLVertexBuffer::MAP_COLOR;

	// Process non-alpha faces
	U32 simple_mask = BASE_MASK | LLVertexBuffer::MAP_NORMAL;

	// hack to give alpha verts their own VBO
	U32 alpha_mask = simple_mask | 0x80000000;

	U32 bump_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD1;

	U32 fullbright_mask = BASE_MASK;

	U32 norm_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD1 |
					LLVertexBuffer::MAP_TANGENT;

	U32 normspec_mask = norm_mask | LLVertexBuffer::MAP_TEXCOORD2;

	U32 spec_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD2;

	U32 pbr_mask = simple_mask | LLVertexBuffer::MAP_TANGENT;

	if (emissive)
	{
		// Emissive faces are present, add emissive bit to preserve batching
		simple_mask |= LLVertexBuffer::MAP_EMISSIVE;
		alpha_mask |= LLVertexBuffer::MAP_EMISSIVE;
		bump_mask |= LLVertexBuffer::MAP_EMISSIVE;
		fullbright_mask |= LLVertexBuffer::MAP_EMISSIVE;
		norm_mask |= LLVertexBuffer::MAP_EMISSIVE;
		normspec_mask |= LLVertexBuffer::MAP_EMISSIVE;
		spec_mask |= LLVertexBuffer::MAP_EMISSIVE;
		pbr_mask |= LLVertexBuffer::MAP_EMISSIVE;
	}

	bool batch_textures =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 1;
	if (batch_textures)
	{
		bump_mask |= LLVertexBuffer::MAP_TANGENT;
		alpha_mask |= LLVertexBuffer::MAP_TANGENT |
					  LLVertexBuffer::MAP_TEXCOORD1 |
					  LLVertexBuffer::MAP_TEXCOORD2;
	}

	U32 extra_mask = 0;
	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		genDrawInfo(groupp, simple_mask | extra_mask, sSimpleFaces[rigged],
					simple_count[rigged], false, batch_textures, rigged);
		genDrawInfo(groupp, fullbright_mask | extra_mask,
					sFullbrightFaces[rigged], fullbright_count[rigged], false,
					batch_textures, rigged);
		genDrawInfo(groupp, alpha_mask | extra_mask, sAlphaFaces[rigged],
					alpha_count[rigged], true, batch_textures, rigged);
		genDrawInfo(groupp, bump_mask | extra_mask, sBumpFaces[rigged],
					bump_count[rigged], false, false, rigged);
		genDrawInfo(groupp, norm_mask | extra_mask, sNormFaces[rigged],
					norm_count[rigged], false, false, rigged);
		genDrawInfo(groupp, spec_mask | extra_mask, sSpecFaces[rigged],
					spec_count[rigged], false, false, rigged);
		genDrawInfo(groupp, normspec_mask | extra_mask, sNormSpecFaces[rigged],
					normspec_count[rigged], false, false, rigged);
		if (gUsePBRShaders)
		{
			genDrawInfo(groupp, pbr_mask | extra_mask, sPbrFaces[rigged],
						pbr_count[rigged], false, false, rigged);
		}
		// For the second pass (rigged), add weights
		extra_mask = LLVertexBuffer::MAP_WEIGHT4;
	}

	// Drawables have been rebuilt, clear rebuild status
	for (LLSpatialGroup::element_iter it = groupp->getDataBegin(),
									  end = groupp->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (drawablep)
		{
			drawablep->clearState(LLDrawable::REBUILD_ALL);
		}
	}

	groupp->mLastUpdateTime = gFrameTimeSeconds;
	groupp->mBuilt = 1.f;
	groupp->clearState(LLSpatialGroup::GEOM_DIRTY |
					   LLSpatialGroup::ALPHA_DIRTY);

	if (voattachavp)
	{
		voattachavp->addAttachmentBytes(groupp->mGeometryBytes);
		voattachavp->addAttachmentArea(groupp->mSurfaceArea);
	}
	if (voavp && voavp != voattachavp)
	{
		voavp->addAttachmentBytes(groupp->mGeometryBytes);
		voavp->addAttachmentArea(groupp->mSurfaceArea);
	}
}

//virtual
void LLVolumeGeometryManager::rebuildMesh(LLSpatialGroup* groupp)
{
	if (!groupp || !groupp->hasState(LLSpatialGroup::MESH_DIRTY) ||
		groupp->hasState(LLSpatialGroup::GEOM_DIRTY))
	{
		llassert(false);
		return;
	}

	LL_FAST_TIMER(FTM_VOLUME_GEOM);

	groupp->mBuilt = 1.f;

	for (LLSpatialGroup::element_iter it = groupp->getDataBegin(),
									  end = groupp->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (!drawablep || drawablep->isDead() ||
			!drawablep->isState(LLDrawable::REBUILD_ALL))
		{
			continue;
		}

		LLVOVolume* vobjp = drawablep->getVOVolume();
		if (!vobjp || vobjp->getLOD() == -1)
		{
			continue;
		}

		LLVolume* volp = vobjp->getVolume();
		if (!volp)
		{
			continue;
		}

		vobjp->preRebuild();

		if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
		{
			vobjp->updateRelativeXform(true);
		}

		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (!face) continue;

			LLVertexBuffer* buffp = face->getVertexBuffer();
			if (!buffp) continue;

			if (!face->getGeometryVolume(*volp, face->getTEOffset(),
										 vobjp->getRelativeXform(),
										 vobjp->getRelativeXformInvTrans(),
										 face->getGeomIndex()))
			{
				// Something gone wrong with the vertex buffer accounting,
				// rebuild this group
				groupp->dirtyGeom();
				gPipeline.markRebuild(groupp);
			}

			buffp->unmapBuffer();
		}

		if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
		{
			vobjp->updateRelativeXform();
		}

		drawablep->clearState(LLDrawable::REBUILD_ALL);
	}

	// Do not forget alpha
	if (groupp)
	{
		LLVertexBuffer* buffp = groupp->mVertexBuffer.get();
		if (buffp)
		{
			buffp->unmapBuffer();
		}
	}

	groupp->clearState(LLSpatialGroup::MESH_DIRTY |
					   LLSpatialGroup::NEW_DRAWINFO);
}

struct CompareBatchBreaker
{
	bool operator()(const LLFace* const& lhs, const LLFace* const& rhs)
	{
		const LLTextureEntry* lte = lhs->getTextureEntry();
		const LLTextureEntry* rte = rhs->getTextureEntry();

		if (lte->getBumpmap() != rte->getBumpmap())
		{
			return lte->getBumpmap() < rte->getBumpmap();
		}
		if (lte->getFullbright() != rte->getFullbright())
		{
			return lte->getFullbright() < rte->getFullbright();
		}
		if (LLPipeline::sRenderDeferred &&
			lte->getMaterialID() != rte->getMaterialID())
		{
			return lte->getMaterialID() < rte->getMaterialID();
		}
		if (lte->getShiny() != rte->getShiny())
		{
			return lte->getShiny() < rte->getShiny();
		}
		if (lhs->getTexture() != rhs->getTexture())
		{
			return lhs->getTexture() < rhs->getTexture();
		}
		// All else being equal, maintain consistent draw order
		return lhs->getDrawOrderIndex() < rhs->getDrawOrderIndex();
	}
};

#if 0	// Not used any more: see below for rigged mesh draw order. HB
struct CompareBatchBreakerRigged
{
	bool operator()(const LLFace* const& lhs, const LLFace* const& rhs)
	{
		const LLTextureEntry* lte = lhs->getTextureEntry();
		const LLTextureEntry* rte = rhs->getTextureEntry();

		if (lhs->mAvatar != rhs->mAvatar)
		{
			return lhs->mAvatar < rhs->mAvatar;
		}
		if (lhs->mSkinInfo->mHash != rhs->mSkinInfo->mHash)
		{
			return lhs->mSkinInfo->mHash < rhs->mSkinInfo->mHash;
		}
		// "Inherit from non-rigged behaviour
		if (lte->getBumpmap() != rte->getBumpmap())
		{
			return lte->getBumpmap() < rte->getBumpmap();
		}
		if (lte->getFullbright() != rte->getFullbright())
		{
			return lte->getFullbright() < rte->getFullbright();
		}
		if (LLPipeline::sRenderDeferred &&
			lte->getMaterialID() != rte->getMaterialID())
		{
			return lte->getMaterialID() < rte->getMaterialID();
		}
		if (lte->getShiny() != rte->getShiny())
		{
			return lte->getShiny() < rte->getShiny();
		}
		if (lhs->getTexture() != rhs->getTexture())
		{
			return lhs->getTexture() < rhs->getTexture();
		}
		// All else being equal, maintain consistent draw order
		return lhs->getDrawOrderIndex() < rhs->getDrawOrderIndex();
	}
};
#endif

struct CompareDrawOrder
{
	LL_INLINE bool operator()(const LLFace* const& lhs,
							  const LLFace* const& rhs)
	{
#if 1	// Still sort by avatar. HB
		if (lhs->mAvatar != rhs->mAvatar)
		{
			return lhs->mAvatar < rhs->mAvatar;
		}
#endif
		return lhs->getDrawOrderIndex() < rhs->getDrawOrderIndex();
	}
};

void LLVolumeGeometryManager::genDrawInfo(LLSpatialGroup* groupp, U32 mask,
										  LLFace** faces, U32 face_count,
										  bool distance_sort,
										  bool batch_textures, bool rigged)
{
	LL_FAST_TIMER(FTM_REBUILD_VOLUME_GEN_DRAW_INFO);

	// Calculate maximum number of vertices to store in a single buffer
	U32 max_vertices =
		(LLVOVolume::sRenderMaxVBOSize * 1024) /
		LLVertexBuffer::calcVertexSize(groupp->getSpatialPartition()->mVertexDataMask);
	max_vertices = llmin(max_vertices, 65535U);

	{
		LL_FAST_TIMER(FTM_GEN_DRAW_INFO_SORT);
		if (rigged)
		{
#if 0		// This does not work: we need to *always* preserve legacy draw
			// order for rigged meshes !  HB
			if (distance_sort)
			{
				// Preserve legacy draw order for rigged faces
				std::sort(faces, faces + face_count, CompareDrawOrder());
			}
			else
			{
				// Sort faces by things that break batches, including avatar
				// and mesh Id
				std::sort(faces, faces + face_count,
						  CompareBatchBreakerRigged());
			}
#else
			// Preserve legacy draw order for rigged faces
			std::sort(faces, faces + face_count, CompareDrawOrder());
#endif
		}
		else if (distance_sort)
		{
			// Sort faces by distance
			std::sort(faces, faces + face_count,
					  LLFace::CompareDistanceGreater());
		}
		else
		{
			// Sort faces by things that break batches
			std::sort(faces, faces + face_count, CompareBatchBreaker());
		}
	}

	bool hud_group = groupp->isHUDGroup();
	LLFace** face_iter = faces;
	LLFace** end_faces = faces + face_count;

	LLSpatialGroup::buffer_map_t buffer_map;

	S32 tex_idx_channels = 1;
	if (gUsePBRShaders)
	{
		tex_idx_channels = LLGLSLShader::sIndexedTextureChannels;
	}
	else if (LLPipeline::sRenderDeferred && distance_sort)
	{
		tex_idx_channels =
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels;
	}
	else
	{
		if (gGLManager.mGLSLVersionMajor > 1 ||
			gGLManager.mGLSLVersionMinor >= 30)
		{
			// - 1 to always reserve one for shiny for now just for simplicity
			tex_idx_channels = LLGLSLShader::sIndexedTextureChannels - 1;
		}
		static LLCachedControl<U32> max_tex_idx(gSavedSettings,
												"RenderMaxTextureIndex");
		tex_idx_channels = llmin(tex_idx_channels, max_tex_idx);
		// NEVER use more than 16 texture index channels (workaround for
		// prevalent driver bug)
		tex_idx_channels = llmin(tex_idx_channels, 16);
	}

//MK
	bool restricted_vision = gRLenabled && gRLInterface.mVisionRestricted;
	F32 cam_dist_max_squared = EXTREMUM;
	LLVector3 joint_pos;
	if (restricted_vision)
	{
		LLJoint* ref_joint = gRLInterface.getCamDistDrawFromJoint();
		if (ref_joint)
		{
			// Calculate the position of the avatar here so we do not have to
			// do it for each face
			joint_pos = ref_joint->getWorldPosition();
			cam_dist_max_squared = gRLInterface.mCamDistDrawMax;
			cam_dist_max_squared *= cam_dist_max_squared;
		}
		else
		{
			restricted_vision = false;
		}
	}
//mk

	bool not_debugging_alpha = !LLDrawPoolAlpha::sShowDebugAlpha;
	while (face_iter != end_faces)
	{
		// Pull off next face
		LLFace* facep = *face_iter;
		if (!facep) continue;	// Paranoia
//MK
		bool is_far_face = false;
		if (restricted_vision)
		{
			LLVector3 face_offset = facep->getPositionAgent() - joint_pos;
			is_far_face = face_offset.lengthSquared() > cam_dist_max_squared;
		}
//mk

		const LLTextureEntry* tep = facep->getTextureEntry();
		if (!tep) continue;	// Paranoia

		LLMaterialPtr matp = tep->getMaterialParams();
		LLMaterialID mat_id = tep->getMaterialID();

		LLViewerTexture* texp = distance_sort ? NULL : facep->getTexture();

		U32 index_count = facep->getIndicesCount();
		U32 geom_count = facep->getGeomCount();

		// Sum up vertices needed for this render batch
		LLFace** i = face_iter;
		++i;

		constexpr U32 MAX_TEXTURE_COUNT = 32;
		LLViewerTexture* texture_list[MAX_TEXTURE_COUNT];

		U32 texture_count = 0;

		{
			LL_FAST_TIMER(FTM_GEN_DRAW_INFO_FACE_SIZE);
			if (batch_textures)
			{
				U8 cur_tex = 0;
				facep->setTextureIndex(cur_tex);
				if (texture_count < MAX_TEXTURE_COUNT)
				{
					texture_list[texture_count++] = texp;
				}

				if (facep->canBatchTexture())
				{
					// Populate texture_list with any textures that can be
					// batched. Move i to the next unbatchable face.
					while (i != end_faces)
					{
						facep = *i;

						if (!facep->canBatchTexture())
						{
							facep->setTextureIndex(0);
							break;
						}

						if (facep->getTexture() != texp)
						{
							if (distance_sort)
							{
								// Textures might be out of order, see if
								// texture exists in current batch
								bool found = false;
								for (U32 tex_idx = 0; tex_idx < texture_count;
									 ++tex_idx)
								{
									if (facep->getTexture() ==
											texture_list[tex_idx])
									{
										cur_tex = tex_idx;
										found = true;
										break;
									}
								}
								if (!found)
								{
									cur_tex = texture_count;
								}
							}
							else
							{
								++cur_tex;
							}

							if (cur_tex >= tex_idx_channels)
							{
								// Cut batches when index channels are depleted
								break;
							}

							texp = facep->getTexture();
							if (texture_count < MAX_TEXTURE_COUNT)
							{
								texture_list[texture_count++] = texp;
							}
						}

						if (geom_count + facep->getGeomCount() > max_vertices)
						{
							// Cut batches on geom count too big
							break;
						}

						++i;
						index_count += facep->getIndicesCount();
						geom_count += facep->getGeomCount();

						facep->setTextureIndex(cur_tex);
					}
				}
				else
				{
					facep->setTextureIndex(0);
				}

				texp = texture_list[0];
			}
			else
			{
				while (i != end_faces &&
					   (distance_sort || (*i)->getTexture() == texp))
				{
					facep = *i;

					const LLTextureEntry* next_tep = facep->getTextureEntry();
					if (next_tep && next_tep->getMaterialID() != mat_id)
					{
						break;
					}

					// Face has no texture index
					facep->mDrawInfo = NULL;
					facep->setTextureIndex(FACE_DO_NOT_BATCH_TEXTURES);

					if (geom_count + facep->getGeomCount() > max_vertices)
					{
						// Cut batches on geom count too big
						break;
					}

					++i;
					index_count += facep->getIndicesCount();
					geom_count += facep->getGeomCount();
				}
			}
		}

		// Create vertex buffer
		LLPointer<LLVertexBuffer> buffp;
		{
			buffp = createVertexBuffer(mask);
			if (!buffp->allocateBuffer(geom_count, index_count))
			{
				llwarns << "Failure to resize a vertex buffer with "
						<< geom_count << " vertices and " << index_count
						<< " indices" << llendl;
				buffp = NULL;
			}
		}
		if (buffp.notNull())
		{
			groupp->mGeometryBytes += buffp->getSize() +
									  buffp->getIndicesSize();
			buffer_map[mask][*face_iter].push_back(buffp);
		}

		// Add face geometry

		U32 indices_index = 0;
		U16 index_offset = 0;

		bool can_use_vertex_shaders = gPipeline.shadersLoaded();

		while (face_iter < i)
		{
			// Update face indices for new buffer
			facep = *face_iter;

			if (buffp.isNull())
			{
				// Bulk allocation failed
				facep->setVertexBuffer(buffp);
				facep->setSize(0, 0); // Mark as no geometry
				++face_iter;
				continue;
			}

			facep->setIndicesIndex(indices_index);
			facep->setGeomIndex(index_offset);
			facep->setVertexBuffer(buffp);

			if (batch_textures &&
				facep->getTextureIndex() == FACE_DO_NOT_BATCH_TEXTURES)
			{
				llwarns_sparse << "Invalid texture index. Skipping." << llendl;
				++face_iter;
				continue;
			}

			// For debugging, set last time face was updated vs moved
			facep->updateRebuildFlags();

			// Copy face geometry into vertex buffer
			LLDrawable* drawablep = facep->getDrawable();
			LLVOVolume* vobjp = drawablep->getVOVolume();
			LLVolume* volp = vobjp->getVolume();

			if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
			{
				vobjp->updateRelativeXform(true);
			}

			U32 te_idx = facep->getTEOffset();

			if (!facep->getGeometryVolume(*volp, te_idx,
										  vobjp->getRelativeXform(),
										  vobjp->getRelativeXformInvTrans(),
										  index_offset, true))
			{
				llwarns << "Failed to get geometry for face !" << llendl;
			}

			if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
			{
				vobjp->updateRelativeXform(false);
			}

			index_offset += facep->getGeomCount();
			indices_index += facep->getIndicesCount();

			// Append face to appropriate render batch

			bool force_simple =
				facep->getPixelArea() < FORCE_SIMPLE_RENDER_AREA;
			bool fullbright = facep->isState(LLFace::FULLBRIGHT);
			if ((mask & LLVertexBuffer::MAP_NORMAL) == 0)
			{
				// Paranoia check to make sure GL does not try to read
				// non-existant normals
				fullbright = true;
			}

			const LLTextureEntry* tep = facep->getTextureEntry();

			LLGLTFMaterial* gltfp = NULL;
			if (gUsePBRShaders)
			{
				gltfp = tep->getGLTFRenderMaterial();
			}
			if (hud_group && !gltfp)
			{
				// All hud attachments are fullbright
				fullbright = true;
			}
			bool is_transparent = transparent_face(gltfp, facep, tep);
			// Do not render transparent faces, unless we highlight transparent
			if (not_debugging_alpha && is_transparent)
			{
				++face_iter;
				continue;
			}

			texp = facep->getTexture();

			bool is_alpha = facep->getPoolType() == LLDrawPool::POOL_ALPHA;

			// Ignore legacy material when PBR material is present
			bool can_be_shiny = !gltfp;
			bool has_glow = tep->hasGlow();
			LLMaterial* matp = gltfp ? NULL : tep->getMaterialParams().get();
			U8 diffuse_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
			if (matp)
			{
				diffuse_mode = matp->getDiffuseAlphaMode();
				can_be_shiny =
					diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE ||
					diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE;
			}

			bool use_legacy_bump = tep->getBumpmap() &&
								   tep->getBumpmap() < 18 &&
								   (!matp || matp->getNormalID().isNull());
			bool is_opaque = opaque_face(facep, tep);
			if (!is_opaque && !is_alpha && !gltfp)
			{
				is_alpha = true;
			}
//MK
			if (restricted_vision)
			{
				// Due to a rendering bug, we must completely ignore the alpha
				// and fullbright of any object (except our own attachments and
				// 100% invisible objects) when the vision is restricted
				LLDrawable* drawablep = facep->getDrawable();
				LLVOVolume* vobjp = drawablep->getVOVolume();
				if ((is_alpha || fullbright) && !is_transparent)
				{
					if (vobjp && vobjp->getAvatar() != gAgentAvatarp)
					{
						// If this is an attachment with alpha or full bright
						// and its wearer is farther than the vision range, do
						// not render it at all
						if (is_far_face && vobjp->isAttachment())
						{
							++face_iter;
							continue;
						}
						else if (vobjp->flagPhantom())
						{
							// If the object is phantom, no need to even render
							// it at all. If it is solid, then a blind avatar
							// will have to "see" it since it may bump into it
							++face_iter;
							continue;
						}
						else if (is_far_face)
						{
							is_alpha = fullbright = can_be_shiny = false;
							is_opaque = true;
						}
					}
				}
				else if (is_transparent && !(vobjp && vobjp->isAttachment()))
				{
					// Completely transparent and not an attachment => do not
					// bother rendering it at all (even when highlighting
					// transparent)
					++face_iter;
					continue;
				}
			}
//mk

			if (gltfp)
			{
				// All other parameters ignored when PBR material is present
				if (gltfp->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
				}
				else if (gltfp->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
				{
					registerFace(groupp, facep,
								 LLRenderPass::PASS_MAT_PBR_ALPHA_MASK);
				}
				else
				{
					registerFace(groupp, facep, LLRenderPass::PASS_MAT_PBR);
				}
			}
			else if (matp && !hud_group && LLPipeline::sRenderDeferred)
			{
				bool material_pass = false;

				// Do NOT use 'fullbright' for this logic or you risk sending
				// things without normals down the materials pipeline and will
				// render poorly if not crash NORSPEC-240,314
				if (tep->getFullbright())
				{
					if (diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
					{
						if (is_opaque)
						{
							registerFace(groupp, facep,
										 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
						}
						else
						{
							registerFace(groupp, facep,
										 LLRenderPass::PASS_ALPHA);
						}
					}
					else if (is_alpha)
					{
						registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
					}
					else if (!restricted_vision &&	// mk
							 (tep->getShiny() > 0 ||
							  matp->getSpecularID().notNull()))
					{
						material_pass = true;
					}
					else if (is_opaque)
					{
						registerFace(groupp, facep,
									 LLRenderPass::PASS_FULLBRIGHT);
					}
					else
					{
						registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
					}
				}
				else if (!is_opaque)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
				}
				else if (use_legacy_bump)
				{
					// We have a material AND legacy bump settings, but no
					// normal map
					registerFace(groupp, facep, LLRenderPass::PASS_BUMP);
				}
				else
				{
					material_pass = true;
				}

				if (material_pass &&
					diffuse_mode != LLMaterial::DIFFUSE_ALPHA_MODE_DEFAULT)
				{
					static const U32 pass[] =
					{
						LLRenderPass::PASS_MATERIAL,
						LLRenderPass::PASS_ALPHA, // PASS_MATERIAL_ALPHA,
						LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
						LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
						LLRenderPass::PASS_SPECMAP,
						LLRenderPass::PASS_ALPHA, // PASS_SPECMAP_BLEND,
						LLRenderPass::PASS_SPECMAP_MASK,
						LLRenderPass::PASS_SPECMAP_EMISSIVE,
						LLRenderPass::PASS_NORMMAP,
						LLRenderPass::PASS_ALPHA, // PASS_NORMMAP_BLEND,
						LLRenderPass::PASS_NORMMAP_MASK,
						LLRenderPass::PASS_NORMMAP_EMISSIVE,
						LLRenderPass::PASS_NORMSPEC,
						LLRenderPass::PASS_ALPHA, // PASS_NORMSPEC_BLEND,
						LLRenderPass::PASS_NORMSPEC_MASK,
						LLRenderPass::PASS_NORMSPEC_EMISSIVE,
					};

					// *HACK: this should never happen, but sometimes we get a
					// material that thinks it has alpha blending when it ought
					// not.
					U8 mode = diffuse_mode;
					if (!distance_sort &&
						mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND)
					{
						mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
					}

					U32 mask = matp->getShaderMask(mode, is_alpha);

					// *HACK: this should also never happen, but sometimes we
					// get here and the material thinks it has a specmap now
					// even though it did not appear to have a specmap when the
					// face was added to the list of faces.
					U32 vb_mask = facep->getVertexBuffer()->getTypeMask();
					if ((mask & LLVertexBuffer::TYPE_TEXCOORD2) &&
						!(vb_mask & LLVertexBuffer::MAP_TEXCOORD2))
					{
						mask &= ~LLVertexBuffer::TYPE_TEXCOORD2;
					}

					mask = llmin(mask, sizeof(pass) / sizeof(U32) - 1);
					registerFace(groupp, facep, pass[mask]);
				}
			}
			else if (matp)
			{
				U8 mode = diffuse_mode;
				is_alpha |= mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				if (is_alpha)
				{
					mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				}

				if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					registerFace(groupp, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK
											: LLRenderPass::PASS_ALPHA_MASK);
				}
				else if (is_alpha)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
				}
				else if (can_be_shiny && can_use_vertex_shaders &&
						 tep->getShiny())
				{
					registerFace(groupp, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT_SHINY
											: LLRenderPass::PASS_SHINY);
				}
				else
				{
					registerFace(groupp, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT
											: LLRenderPass::PASS_SIMPLE);
				}
			}
			else if (is_alpha)
			{
				// When the face itself is 100% transparent, do not render
				// unless we are highlighting transparent
				if (not_debugging_alpha && !has_glow &&
					facep->getRenderColor().mV[3] < 0.001f)
				{
					++face_iter;
					continue;
				}
				// Can we safely treat this as an alpha mask ?
				else if (facep->canRenderAsMask() &&
						 !(gUsePBRShaders && hud_group))
				{
					if (tep->getFullbright())
					{
						registerFace(groupp, facep,
									 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
					}
					else
					{
						registerFace(groupp, facep,
									 LLRenderPass::PASS_ALPHA_MASK);
					}
				}
				else
				{
					registerFace(groupp, facep, LLRenderPass::PASS_ALPHA);
				}
			}
			else if (can_be_shiny && can_use_vertex_shaders && tep->getShiny())
			{
				// Shiny
				if (texp->getPrimaryFormat() == GL_ALPHA)
				{
					// Invisiprim + shiny
					registerFace(groupp, facep,
								 LLRenderPass::PASS_INVISI_SHINY);
					registerFace(groupp, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (!hud_group && LLPipeline::sRenderDeferred)
				{
					// Deferred rendering
					if (tep->getFullbright())
					{
						// Register in post deferred fullbright shiny pass
						registerFace(groupp, facep,
									 LLRenderPass::PASS_FULLBRIGHT_SHINY);
						if (tep->getBumpmap())
						{
							// Register in post deferred bump pass
							registerFace(groupp, facep,
										 LLRenderPass::PASS_POST_BUMP);
						}
					}
					else if (use_legacy_bump)
					{
						// Register in deferred bump pass
						registerFace(groupp, facep, LLRenderPass::PASS_BUMP);
					}
					else
					{
						// Register in deferred simple pass (deferred simple
						// includes shiny)
						llassert(mask & LLVertexBuffer::MAP_NORMAL);
						registerFace(groupp, facep, LLRenderPass::PASS_SIMPLE);
					}
				}
				else if (fullbright)
				{
					// Not deferred, register in standard fullbright shiny pass
					registerFace(groupp, facep,
								 LLRenderPass::PASS_FULLBRIGHT_SHINY);
				}
				else
				{
					// Not deferred or fullbright, register in standard shiny
					// pass
					registerFace(groupp, facep, LLRenderPass::PASS_SHINY);
				}
			}
			else	// Not alpha and not shiny
			{
				if (!is_alpha && texp->getPrimaryFormat() == GL_ALPHA)
				{
					// Invisiprim
					registerFace(groupp, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (fullbright)
				{
					// Fullbright
					if (matp &&
						diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
					{
						registerFace(groupp, facep,
									 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
					}
					else
					{
						registerFace(groupp, facep,
									 LLRenderPass::PASS_FULLBRIGHT);
					}
					if (!hud_group && LLPipeline::sRenderDeferred &&
						use_legacy_bump)
					{
						// If this is the deferred render and a bump map is
						// present, register in post deferred bump
						registerFace(groupp, facep,
									 LLRenderPass::PASS_POST_BUMP);
					}
				}
				else if (LLPipeline::sRenderDeferred && use_legacy_bump)
				{
					// Non-shiny or fullbright deferred bump
					registerFace(groupp, facep, LLRenderPass::PASS_BUMP);
				}
				// All around simple from there
				else if (matp &&
						 diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					llassert(mask & LLVertexBuffer::MAP_NORMAL);
					// Material alpha mask can be respected in non-deferred
					registerFace(groupp, facep, LLRenderPass::PASS_ALPHA_MASK);
				}
				else
				{
					llassert(mask & LLVertexBuffer::MAP_NORMAL);
					registerFace(groupp, facep, LLRenderPass::PASS_SIMPLE);
				}

				if (!can_use_vertex_shaders && !is_alpha && tep->getShiny())
				{
					// Shiny has an extra pass when shaders are disabled
					registerFace(groupp, facep, LLRenderPass::PASS_SHINY);
				}
			}

			// Not sure why this is here, and looks like it might cause bump
			// mapped objects to get rendered redundantly -- davep 5/11/2010
			if (!is_alpha && (hud_group || !LLPipeline::sRenderDeferred))
			{
				llassert((mask & LLVertexBuffer::MAP_NORMAL) || fullbright);
				facep->setPoolType(fullbright ? LLDrawPool::POOL_FULLBRIGHT
											  : LLDrawPool::POOL_SIMPLE);

				if (!force_simple && use_legacy_bump)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_BUMP);
				}
			}

			if (!is_alpha && has_glow && LLPipeline::RenderGlow)
			{
//MK
				if (is_far_face)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_SIMPLE);
				}
				else
//mk
				if (gltfp)
				{
					registerFace(groupp, facep, LLRenderPass::PASS_PBR_GLOW);
				}
				else
				{
					registerFace(groupp, facep, LLRenderPass::PASS_GLOW);
				}
			}

			++face_iter;
		}

		if (buffp.notNull())
		{
			buffp->unmapBuffer();
		}
	}

	// Replace old buffer map with the new one (swapping them is by far the
	// fastest way to do this). HB
	groupp->mBufferMap[mask].swap(buffer_map[mask]);
}

//virtual
void LLGeometryManager::addGeometryCount(LLSpatialGroup* groupp,
										 U32& vertex_count, U32& index_count)
{
	// Clear off any old faces
	mFaceList.clear();

	// For each drawable
	for (LLSpatialGroup::element_iter it = groupp->getDataBegin(),
									  end = groupp->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		// For each face
		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			// Sum up face verts and indices
			drawablep->updateFaceSize(i);
			LLFace* facep = drawablep->getFace(i);
			if (facep)
			{
				if (facep->hasGeometry() &&
					facep->getPixelArea() > FORCE_CULL_AREA &&
					facep->getGeomCount() + vertex_count <= 65536)
				{
					vertex_count += facep->getGeomCount();
					index_count += facep->getIndicesCount();

					// Remember face (for sorting)
					mFaceList.push_back(facep);
				}
				else
				{
					facep->clearVertexBuffer();
				}
			}
		}
	}
}

LLHUDPartition::LLHUDPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mPartitionType = LLViewerRegion::PARTITION_HUD;
	mDrawableType = LLPipeline::RENDER_TYPE_HUD;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}
