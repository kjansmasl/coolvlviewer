/**
 * @file llviewertexture.h
 * @brief Object for managing images and their textures
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

#ifndef LL_LLVIEWERTEXTURE_H
#define LL_LLVIEWERTEXTURE_H

#include <list>

#include "llatomic.h"
#include "llcorehttpcommon.h"
#include "hbfastmap.h"
#include "llgltexture.h"
#include "llhost.h"
#include "llrender.h"
#include "lltimer.h"
#include "lluuid.h"

#include "llface.h"

class LLImageGL;
class LLImageRaw;
class LLMessageSystem;
class LLViewerObject;
class LLViewerTexture;
class LLViewerFetchedTexture;
class LLViewerMediaTexture;
class LLVOVolume;
class LLViewerMediaImpl;

typedef	void (*loaded_callback_func)(bool success,
									 LLViewerFetchedTexture* src_vi,
									 LLImageRaw* srcp,
									 LLImageRaw* src_auxp,
									 S32 discard_level,
									 bool final,
									 void* userdata);

class LLLoadedCallbackEntry
{
public:
	LLLoadedCallbackEntry(loaded_callback_func cb,
						  S32 discard_level,
						  // whether we need image raw for the callback
						  bool need_imageraw,
						  void* userdata, uuid_list_t* src_cb_list,
						  LLViewerFetchedTexture* target, bool pause);

	void removeTexture(LLViewerFetchedTexture* tex);

	loaded_callback_func	mCallback;
	S32						mLastUsedDiscard;
	S32						mDesiredDiscard;
	bool					mNeedsImageRaw;
	bool				    mPaused;
	void*					mUserData;
	uuid_list_t*			mSourceCallbackList;

public:
	static void cleanUpCallbackList(uuid_list_t* cb_list);
};

class LLTextureBar;

class LLViewerTexture : public LLGLTexture
{
	friend class LLBumpImageList;
	friend class LLUIImageList;
	friend class LLTextureBar;

protected:
	LOG_CLASS(LLViewerTexture);

	~LLViewerTexture() override;

	void cleanup();
	void init(bool firstinit);

	void reorganizeFaceList();
	void reorganizeVolumeList();

#if LL_FIX_MAT_TRANSPARENCY
	void notifyAboutMissingAsset();
	void notifyAboutCreatingTexture();
#endif

public:
	enum
	{
		LOCAL_TEXTURE,
		MEDIA_TEXTURE,
		DYNAMIC_TEXTURE,
		FETCHED_TEXTURE,
		LOD_TEXTURE,
		INVALID_TEXTURE_TYPE
	};

	typedef std::vector<LLFace*> ll_face_list_t;
	typedef std::vector<LLVOVolume*> ll_volume_list_t;

	static void initClass();
	static void updateClass();

	LLViewerTexture(bool usemipmaps = true);
	LLViewerTexture(const LLUUID& id, bool usemipmaps);
	LLViewerTexture(const LLImageRaw* rawp, bool usemipmaps);
	LLViewerTexture(U32 width, U32 height, U8 components, bool usemipmaps);

	void setNeedsAlphaAndPickMask(bool b);

	S8 getType() const override;

	LL_INLINE virtual bool isViewerMediaTexture() const		{ return false; }

	LL_INLINE virtual bool isMissingAsset() const			{ return false; }

	void dump() override;	// Debug info to llinfos

	bool bindDefaultImage(S32 stage = 0) override;
	LL_INLINE void forceImmediateUpdate() override			{}

	LL_INLINE const LLUUID& getID() const override			{ return mID; }

	void setBoostLevel(U32 level) override;

	void addTextureStats(F32 virtual_size,
						 bool needs_gltexture = true) const;
	void resetTextureStats();

	LL_INLINE void setMaxVirtualSizeResetInterval(S32 interval) const
	{
		mMaxVirtualSizeResetInterval = interval;
	}

	LL_INLINE void resetMaxVirtualSizeResetCounter() const
	{
		mMaxVirtualSizeResetCounter = mMaxVirtualSizeResetInterval;
	}

	LL_INLINE virtual F32 getMaxVirtualSize()				{ return mMaxVirtualSize; }

	void resetLastReferencedTime();
	F32 getElapsedLastReferenceTime();

	LL_INLINE void setKnownDrawSize(S32, S32) override		{}

	virtual void addFace(U32 channel, LLFace* facep);
	virtual void removeFace(U32 channel, LLFace* facep);
	S32 getTotalNumFaces() const;
	S32 getNumFaces(U32 ch) const;

	LL_INLINE const ll_face_list_t* getFaceList(U32 ch) const
	{
		return ch < LLRender::NUM_TEXTURE_CHANNELS ? &mFaceList[ch] : NULL;
	}

	virtual void addVolume(U32 ch, LLVOVolume* volumep);
	virtual void removeVolume(U32 ch, LLVOVolume* volumep);

	LL_INLINE S32 getNumVolumes(U32 ch) const				{ return mNumVolumes[ch]; }

	LL_INLINE const ll_volume_list_t* getVolumeList(U32 ch) const
	{
		return &mVolumeList[ch];
	}

	LL_INLINE virtual void setCachedRawImage(S32 discard_level,
											 LLImageRaw* rawp)
	{
	}

	LL_INLINE bool isLargeImage()							
	{
		return mTexelsPerImage > sMinLargeImageSize;
	}

	LL_INLINE void setParcelMedia(LLViewerMediaTexture* m)	{ mParcelMedia = m; }
	LL_INLINE bool hasParcelMedia() const					{ return mParcelMedia != NULL; }
	LL_INLINE LLViewerMediaTexture* getParcelMedia() const	{ return mParcelMedia; }

	LL_INLINE static bool inLowMemCondition()
	{
		constexpr F32 LOW_MEM_COND_DURATION = 30.f;
		return sCurrentTime < sLastLowMemCondTime + LOW_MEM_COND_DURATION;
	}

	// Called on sim change, to start texture loading afresh. With 'reset_bias'
	// set to true (used on far TP arrival), the discard bias is reset below
	// the limit at which the texture boost is cancelled. HB
	static void resetLowMemCondition(bool reset_bias = false);

private:
	LL_INLINE virtual void switchToCachedImage()			{}

	// Returns false when enough VRAM (and discard unchanged), or true when
	// short on VRAM, with discard updated. can_decrease_bias is set to false
	// whenever the algorithm predicts too tight a memory conditions on next
	// check (even if this check passed and true is returned) and, of course,
	// whenever the method returns true. HB
	static bool isMemoryForTextureLow(F32& discard, bool& can_decrease_bias);

protected:
	// Do not use LLPointer here.
	LLViewerMediaTexture*	mParcelMedia;

	LLUUID					mID;

	// The largest virtual size of the image, in pixels: how much data to we
	// need ?
	mutable F32				mMaxVirtualSize;
	mutable S32				mMaxVirtualSizeResetCounter;
	mutable S32				mMaxVirtualSizeResetInterval;
	// Priority adding to mDecodePriority.
	mutable F32				mAdditionalDecodePriority;

	F32						mLastReferencedTime;
	F32						mLastFaceListUpdate;
	F32						mLastVolumeListUpdate;

	U32						mNumFaces[LLRender::NUM_TEXTURE_CHANNELS];
	U32						mNumVolumes[LLRender::NUM_VOLUME_TEXTURE_CHANNELS];

	// Reverse pointer pointing to the faces using this image as texture
	ll_face_list_t			mFaceList[LLRender::NUM_TEXTURE_CHANNELS];

	ll_volume_list_t		mVolumeList[LLRender::NUM_VOLUME_TEXTURE_CHANNELS];

public:
	// "Null" texture for non-textured objects.
	static LLPointer<LLViewerTexture> sNullImagep;
	// Texture for rezzing avatars particle cloud
	static LLPointer<LLViewerTexture> sCloudImagep;

	static S32				sImageCount;
	static S32				sRawCount;
	static S32				sAuxCount;
	static F32				sDesiredDiscardBias;
	static S32				sBoundTexMemoryMB;
	static S32				sLastBoundTexMemoryMB;
	static S32				sTotalTexMemoryMB;
	static S32				sLastTotalTexMemoryMB;
	static S32				sMaxBoundTexMemMB;
	static S32				sMaxTotalTexMemMB;
	static S32				sLastFreeVRAMMB;
	static S32				sMinLargeImageSize;
	static S32				sMaxSmallImageSize;
	static F32				sCurrentTime;
	static F32				sNextDiscardBiasUpdateTime;
	static F32				sLastDiscardDecreaseTime;
	static F32				sLastLowMemCondTime;
	static bool				sALMTexPenalty;
	static bool				sDontLoadVolumeTextures;
};

constexpr S32 MAX_SCULPT_REZ = 128; // Maximum sculpt image size

enum FTType
{
	FTT_UNKNOWN = -1,
	FTT_DEFAULT = 0,	// Standard texture fetched by Id.
	FTT_SERVER_BAKE,	// Texture fetched from the baking service.
	FTT_HOST_BAKE,		// Baked tex uploaded by viewer and fetched from sim.
	FTT_MAP_TILE,		// Tiles fetched from map server.
	FTT_LOCAL_FILE		// Fetched directly from a local file.
};

const std::string& fttype_to_string(const FTType& fttype);

//
// Textures are managed in gTextureList. Raw image data is fetched from remote
// or local cache but the raw image this texture pointing to is fixed.
//
class LLViewerFetchedTexture : public LLViewerTexture
{
	friend class LLTextureBar;	// Debug info only
	friend class LLTextureView;	// Debug info only

protected:
	~LLViewerFetchedTexture() override;

public:
	LLViewerFetchedTexture(const LLUUID& id, FTType f_type,
						   const LLHost& host = LLHost(),
						   bool usemipmaps = true);
	LLViewerFetchedTexture(const LLImageRaw* rawp, FTType f_type,
						   bool usemipmaps);
	LLViewerFetchedTexture(const std::string& url, FTType f_type,
						   const LLUUID& id, bool usemipmaps = true);

	LL_INLINE LLViewerFetchedTexture* asFetched() override
	{
		return this;
	}

	static F32 maxDecodePriority();

	struct Compare
	{
		// lhs < rhs
		LL_INLINE bool operator()(const LLPointer<LLViewerFetchedTexture>& lhs,
								  const LLPointer<LLViewerFetchedTexture>& rhs) const
		{
			const LLViewerFetchedTexture* lhsp = (const LLViewerFetchedTexture*)lhs;
			const LLViewerFetchedTexture* rhsp = (const LLViewerFetchedTexture*)rhs;
			// Greater priority is "less"
			const F32 lpriority = lhsp->getDecodePriority();
			const F32 rpriority = rhsp->getDecodePriority();
			if (lpriority > rpriority) // Higher priority
			{
				return true;
			}
			if (lpriority < rpriority)
			{
				return false;
			}
			return lhsp < rhsp;
		}
	};

	S8 getType() const override;
	LL_INLINE FTType getFTType() const						{ return mFTType; }
	void forceImmediateUpdate() override;
	void dump() override;

	// Set callbacks to get called when the image gets updated with higher
	// resolution versions.
	void setLoadedCallback(loaded_callback_func cb, S32 discard_level,
						   bool keep_imageraw, bool needs_aux, void* userdata,
						   uuid_list_t* cbs, bool pause = false);
	LL_INLINE bool hasCallbacks()							{ return !mLoadedCallbackList.empty(); }
	void pauseLoadedCallbacks(const uuid_list_t* cb_list);
	void unpauseLoadedCallbacks(const uuid_list_t* cb_list);
	bool doLoadedCallbacks();
	void deleteCallbackEntry(const uuid_list_t* cb_list);
	void clearCallbackEntryList();

	void addToCreateTexture();

	// ONLY call from LLViewerTextureList to determine if a call to
	// createTexture() is necessary.
	bool preCreateTexture(S32 usename = 0);
	// ONLY call from LLViewerTextureList or ImageGL background thread
	bool createTexture(S32 usename = 0);
	void postCreateTexture();
	void scheduleCreateTexture();

	// Returns false when we cannot delete it, i.e. when mNeedsCreateTexture is
	// true. HB
	bool destroyTexture();

	virtual void processTextureStats();
	F32 calcDecodePriority();

	LL_INLINE bool needsAux() const							{ return mNeedsAux; }

	// Host we think might have this image, used for baked av textures.
	LL_INLINE void setTargetHost(LLHost host)				{ mTargetHost = host; }
	LL_INLINE LLHost getTargetHost() const					{ return mTargetHost; }

	// Set the decode priority for this image...
	// DON'T CALL THIS UNLESS YOU KNOW WHAT YOU'RE DOING, it can mess up
	// the priority list, and cause horrible things to happen.
	void setDecodePriority(F32 priority = -1.0f);
	LL_INLINE F32 getDecodePriority() const					{ return mDecodePriority; };

	void setAdditionalDecodePriority(F32 priority);

	void updateVirtualSize();

	LL_INLINE S32 getDesiredDiscardLevel()					{ return mDesiredDiscardLevel; }
	LL_INLINE void setMinDiscardLevel(S32 discard)			{ mMinDesiredDiscardLevel = llmin(mMinDesiredDiscardLevel,(S8)discard); }

	void setBoostLevel(U32 level) override;

	bool updateFetch();

	void clearFetchedResults();	// Clear all fetched results, for debug use.

	// Override the computation of discard levels if we know the exact output
	// size of the image.  Used for UI textures to not decode, even if we have
	// more data.
	void setKnownDrawSize(S32 width, S32 height) override;

	void setIsMissingAsset(bool is_missing = true);
	LL_INLINE bool isMissingAsset() const override			{ return mIsMissingAsset; }

	// returns dimensions of original image for local files (before power of two scaling)
	// and returns 0 for all asset system images
	LL_INLINE S32 getOriginalWidth()						{ return mOrigWidth; }
	LL_INLINE S32 getOriginalHeight()						{ return mOrigHeight; }

	LL_INLINE bool isInImageList() const					{ return mInImageList; }
	LL_INLINE void setInImageList(bool flag)				{ mInImageList = flag; }

	LL_INLINE U32 getFetchPriority() const					{ return mFetchPriority; }
	LL_INLINE F32 getDownloadProgress() const				{ return mDownloadProgress; }

	LLImageRaw* reloadRawImage(S8 discard_level);
	void destroyRawImage();
	LL_INLINE bool needsToSaveRawImage()					{ return mForceToSaveRawImage || mSaveRawImage; }

	LL_INLINE const std::string& getUrl() const				{ return mUrl; }
	LL_INLINE void setUrl(const std::string& url)			{ mUrl = url; }

	void setDeletionCandidate();
	void setInactive();

	LL_INLINE bool isDeleted()								{ return mTextureState == DELETED; }
	LL_INLINE bool isInactive()								{ return mTextureState == INACTIVE; }
	LL_INLINE bool isDeletionCandidate()					{ return mTextureState == DELETION_CANDIDATE; }
	LL_INLINE bool getUseDiscard() const					{ return mUseMipMaps && !mDontDiscard; }

	void setForSculpt();
	LL_INLINE bool forSculpt() const						{ return mForSculpt; }
	LL_INLINE bool isForSculptOnly() const					{ return mForSculpt && !mNeedsGLTexture; }

	//raw image management
	void checkCachedRawSculptImage();
	LL_INLINE LLImageRaw* getRawImage() const				{ return mRawImage; }
	LL_INLINE S32 getRawImageLevel() const					{ return mRawDiscardLevel; }
	LL_INLINE LLImageRaw* getCachedRawImage() const			{ return mCachedRawImage;}
	LL_INLINE S32 getCachedRawImageLevel() const			{ return mCachedRawDiscardLevel; }
	LL_INLINE bool isCachedRawImageReady() const			{ return mCachedRawImageReady; }
	LL_INLINE bool isRawImageValid() const					{ return mIsRawImageValid; }
	void forceToSaveRawImage(S32 desired_discard = 0, F32 kept_time = 0.f);
	void forceToRefetchTexture(S32 desired_discard = 0, F32 kept_time = 60.f);

	void setCachedRawImage(S32 discard_level, LLImageRaw* rawp) override;

	void destroySavedRawImage();
	LLImageRaw* getSavedRawImage();
	LL_INLINE bool hasSavedRawImage() const					{ return mSavedRawImage.notNull(); }

	F32 getElapsedLastReferencedSavedRawImageTime() const;
	bool isFullyLoaded() const;

	LL_INLINE bool hasFetcher() const						{ return mHasFetcher;}
	LL_INLINE void setCanUseHTTP(bool can_use_http)			{ mCanUseHTTP = can_use_http; }

	// Forces to re-fetch an image (for corrupted ones). HB
	void forceRefetch();
	// Mark request for fetch as deleted to avoid false missing asset issues.
	// HB
	void requestWasDeleted();

protected:
	void switchToCachedImage() override;
	S32 getCurrentDiscardLevelForFetching();

private:
	void init(bool firstinit);
	void cleanup();

	void saveRawImage();
	void setCachedRawImage();
	void setCachedRawImagePtr(LLImageRaw* rawp);

public:
	// Texture to show NOTHING (whiteness)
	static LLPointer<LLViewerFetchedTexture> sWhiteImagep;
	// "Default" texture for error cases, the only case of fetched texture
	// which is generated in local.
	static LLPointer<LLViewerFetchedTexture> sDefaultImagep;
	// Old "Default" translucent texture
	static LLPointer<LLViewerFetchedTexture> sSmokeImagep;
	// Flat normal map denoting no bumpiness on a surface
	static LLPointer<LLViewerFetchedTexture> sFlatNormalImagep;
	// Default Sun image
	static LLPointer<LLViewerFetchedTexture> sDefaultSunImagep;
	// Default Moon image
	static LLPointer<LLViewerFetchedTexture> sDefaultMoonImagep;
	// Default Clouds image
	static LLPointer<LLViewerFetchedTexture> sDefaultCloudsImagep;
	// Cloud noise image
	static LLPointer<LLViewerFetchedTexture> sDefaultCloudNoiseImagep;
	// Bloom image
	static LLPointer<LLViewerFetchedTexture> sBloomImagep;
	// Opaque water image
	static LLPointer<LLViewerFetchedTexture> sOpaqueWaterImagep;
	// Transparent water image
	static LLPointer<LLViewerFetchedTexture> sWaterImagep;
	// Water normal map
	static LLPointer<LLViewerFetchedTexture> sWaterNormapMapImagep;
	// PBR irradiance
	static LLPointer<LLViewerFetchedTexture> sDefaultIrradiancePBRp;

	// Stats (used in lltextureview.cpp)
	static U32				sMainThreadCreations;
	static U32				sImageThreadCreations;
	static U32				sImageThreadQueueSize;
	static bool				sImageThreadCreationsCapped;

protected:
	S32						mOrigWidth;
	S32						mOrigHeight;

	// Override the computation of discard levels if we know the exact output
	// size of the image. Used for UI textures to not decode, even if we have
	// more data.
	S32						mKnownDrawWidth;
	S32						mKnownDrawHeight;

	S32						mRequestedDiscardLevel;
	F32						mRequestedDownloadPriority;
	S32						mFetchState;
	U32						mFetchPriority;
	F32						mDownloadProgress;
	F32						mFetchDeltaTime;
	F32						mRequestDeltaTime;

	// The priority for decoding this image.
	F32						mDecodePriority;

	S32						mMinDiscardLevel;

	// The discard level we'd LIKE to have - if we have it and there's space
	S8						mDesiredDiscardLevel;

	// The minimum discard level we would like to have
	S8						mMinDesiredDiscardLevel;

	// Result of the most recently completed http request for this texture.
	LLCore::HttpStatus		mLastHttpGetStatus;

	// If invalid, just request from agent's simulator
	LLHost					mTargetHost;

	FTType					mFTType;	// What category of image is this ?

	typedef std::list<LLLoadedCallbackEntry*> callback_list_t;
	callback_list_t			mLoadedCallbackList;
	F32						mLastCallBackActiveTime;

	LLPointer<LLImageRaw>	mRawImage;
	// Used ONLY for cloth meshes right now.  Make SURE you know what you're
	// doing if you use it for anything else! - djs
	LLPointer<LLImageRaw>	mAuxRawImage;
	LLPointer<LLImageRaw>	mSavedRawImage;
	// A small version of the copy of the raw image (<= 64 * 64)
	LLPointer<LLImageRaw>	mCachedRawImage;
	S32						mCachedRawDiscardLevel;

	S32						mRawDiscardLevel;
	S32						mSavedRawDiscardLevel;
	S32						mDesiredSavedRawDiscardLevel;
	F32						mLastReferencedSavedRawImageTime;
	F32						mKeptSavedRawImageTime;

	// Timing
	// Last time a packet was received.
	F32						mLastPacketTime;
	// Last time mDecodePriority was zeroed.
	F32						mStopFetchingTime;

	S8						mLoadedCallbackDesiredDiscardLevel;

	// true if we know that there is no image asset with this image id in the
	// database.
	mutable bool			mIsMissingAsset;
	// true if we deleted the fetch request in flight: used to avoid false
	// missing asset cases.
	mutable bool			mWasDeleted;

	// We need to decode the auxiliary channels
	bool					mNeedsAux;
	bool					mHasAux;			// We do have aux channels
	bool					mIsRawImageValid;
	bool					mHasFetcher;		// We have made a fecth request
	bool					mIsFetching;		// Fetch request is active

	// This texture can be fetched through http if true.
	bool					mCanUseHTTP;

	bool					mKnownDrawSizeChanged;

	bool					mPauseLoadedCallBacks;

	// Keep a copy of mRawImage for some special purposes when
	// mForceToSaveRawImage is set.
	bool					mForceToSaveRawImage;
	bool					mSaveRawImage;

	// The rez of the mCachedRawImage reaches the upper limit.
	bool					mCachedRawImageReady;

	// A flag if the texture is used as sculpt data.
	bool					mForSculpt;

	// true if image is in list (in which case do not reset priority !)
	bool					mInImageList;
	// This needs to be atomic, since it is written both in the main thread
	// and in the GL image worker thread... HB
	LLAtomicBool			mNeedsCreateTexture;

	std::string				mLocalFileName;
	std::string				mUrl;

private:
	bool					mFullyLoaded;
	bool					mForceCallbackFetch;
};

//
// The image data is fetched from remote or from local cache. The resolution of
// the texture is adjustable: depends on the view-dependent parameters.
//
class LLViewerLODTexture final : public LLViewerFetchedTexture
{
protected:
	~LLViewerLODTexture() override = default;

public:
	LLViewerLODTexture(const LLUUID& id, FTType f_type,
					   const LLHost& host = LLHost(),
					   bool usemipmaps = true);
	LLViewerLODTexture(const std::string& url, FTType f_type,
					   const LLUUID& id, bool usemipmaps = true);

	S8 getType() const override;
	// Process image stats to determine priority/quality requirements.
	void processTextureStats() override;
	bool isUpdateFrozen();

private:
	void init(bool firstinit);
	bool scaleDown();

private:
	// Virtual size used to calculate desired discard
	F32 mDiscardVirtualSize;
	// Last calculated discard level
	F32 mCalculatedDiscardLevel;
};

//
// The image data is fetched from the media pipeline periodically; the
// resolution of the texture is also adjusted by the media pipeline.
//
class LLViewerMediaTexture final : public LLViewerTexture
{
protected:
	~LLViewerMediaTexture() override;

public:
	LLViewerMediaTexture(const LLUUID& id, bool usemipmaps = true,
						 LLImageGL* gl_image = NULL);

	S8 getType() const override;

	void reinit(bool usemipmaps = true);

	LL_INLINE bool getUseMipMaps()							{ return mUseMipMaps; }
	void setUseMipMaps(bool mipmap);

	void setPlaying(bool playing);
	LL_INLINE bool isPlaying() const						{ return mIsPlaying; }
	void setMediaImpl();

	void initVirtualSize();
	void invalidateMediaImpl();

	void addMediaToFace(LLFace* facep);
	void removeMediaFromFace(LLFace* facep);

	LL_INLINE bool isViewerMediaTexture() const override	{ return true; }

	void addFace(U32 ch, LLFace* facep) override;
	void removeFace(U32 ch, LLFace* facep) override;

	F32 getMaxVirtualSize() override;

	static void updateClass();
	static void cleanUpClass();

	static LLViewerMediaTexture* findMediaTexture(const LLUUID& media_id);
	static void removeMediaImplFromTexture(const LLUUID& media_id);

private:
	void switchTexture(U32 ch, LLFace* facep);
	bool findFaces();
	void stopPlaying();

private:
	// An instant list, recording all faces referencing or can reference to
	// this media texture. NOTE: it is NOT thread safe.
	std::list<LLFace*>	mMediaFaceList;

	// An instant list keeping all textures which are replaced by the current
	// media texture, is only used to avoid the removal of those textures from
	// memory.
	std::list<LLPointer<LLViewerTexture> > mTextureList;

	LLViewerMediaImpl*	mMediaImplp;
	U32					mUpdateVirtualSizeTime;
	bool				mIsPlaying;

	typedef fast_hmap<LLUUID, LLPointer<LLViewerMediaTexture> > media_map_t;
	static media_map_t	sMediaMap;
};

// Purely static class
class LLViewerTextureManager
{
	LLViewerTextureManager() = delete;
	~LLViewerTextureManager() = delete;

public:
	// Returns NULL if tex is not a LLViewerFetchedTexture nor derived from
	// LLViewerFetchedTexture.
	static LLViewerFetchedTexture* staticCast(LLGLTexture* tex,
											  bool report_error = false);

	// "find-texture" just check if the texture exists, if yes, return it,
	// otherwise return null.

	static LLViewerTexture* findTexture(const LLUUID& id);
	static LLViewerMediaTexture* findMediaTexture(const LLUUID& id);

	static LLViewerMediaTexture* createMediaTexture(const LLUUID& id,
													bool usemipmaps = true,
													LLImageGL* gl_image = NULL);

	// get*Texture*() methods create a new texture when it does not exist.

	// NOTE: removed "usemipmaps" parameter since media texture always ended up
	// with mipmap generation turned off. *TODO: consider allowing it again and
	// actually using it in the future. HB
	static LLViewerMediaTexture* getMediaTexture(const LLUUID& id,
												 LLImageGL* gl_image = NULL);

	static LLPointer<LLViewerTexture> getLocalTexture(bool usemipmaps = true,
													  bool generate_gl_tex = true);
	static LLPointer<LLViewerTexture> getLocalTexture(const LLUUID& id,
													  bool usemipmaps,
													  bool generate_gl_tex = true);
	static LLPointer<LLViewerTexture> getLocalTexture(const LLImageRaw* rawp,
													  bool usemipmaps);
	static LLPointer<LLViewerTexture> getLocalTexture(U32 width, U32 height,
													  U8 components,
													  bool usemipmaps,
													  bool generate_gl_tex = true);

	static LLViewerFetchedTexture* getFetchedTexture(const LLImageRaw* rawp,
													 FTType type = FTT_LOCAL_FILE,
													 bool usemipmaps = true);

	static LLViewerFetchedTexture* getFetchedTexture(const LLUUID& image_id,
													 FTType f_type = FTT_DEFAULT,
													 bool usemipmap = true,
													 // Get the requested level immediately upon creation.
													 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
													 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
													 S32 internal_format = 0,
													 U32 primary_format = 0,
													 LLHost request_from_host = LLHost());

	static LLViewerFetchedTexture* getFetchedTextureFromFile(const std::string& filename,
															 bool usemipmap = true,
															 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_UI,
															 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
															 S32 internal_format = 0,
															 U32 primary_format = 0,
															 const LLUUID& force_id = LLUUID::null);

	static LLViewerFetchedTexture* getFetchedTextureFromUrl(const std::string& url,
															FTType f_type,
															bool usemipmap = true,
															LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
															S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
															S32 internal_format = 0,
															U32 primary_format = 0,
															const LLUUID& force_id = LLUUID::null);

	static LLViewerFetchedTexture* getFetchedTextureFromHost(const LLUUID& image_id,
															 FTType f_type,
															 LLHost host);

	static void init();
	static void cleanup();
};

#endif
