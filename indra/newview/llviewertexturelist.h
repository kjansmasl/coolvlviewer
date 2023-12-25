/**
 * @file llviewertexturelist.h
 * @brief Object for managing the list of images within a region
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

#ifndef LL_LLVIEWERTEXTURELIST_H
#define LL_LLVIEWERTEXTURELIST_H

#include <set>
#include <map>

#include "hbfastset.h"
#include "llstat.h"
#include "llstring.h"			// For hash_value(const std::string&)
#include "llui.h"
#include "llworkqueue.h"

#include "llviewertexture.h"

constexpr U32 LL_IMAGE_REZ_LOSSLESS_CUTOFF = 128;

constexpr bool MIPMAP_YES = true;
constexpr bool MIPMAP_NO = false;

constexpr bool GL_TEXTURE_YES = true;
constexpr bool GL_TEXTURE_NO = false;

constexpr bool IMMEDIATE_YES = true;
constexpr bool IMMEDIATE_NO = false;

class LLImageJ2C;
class LLMessageSystem;

typedef	void (*LLImageCallback)(bool success, LLViewerFetchedTexture* texp,
								LLImageRaw* imagep, LLImageRaw* aux_imagep,
								S32 discard_level, bool is_final,
								void* userdata);

class LLViewerTextureList
{
	friend class LLLocalBitmap;
	friend class LLTextureView;
	friend class LLViewerTextureManager;

protected:
	LOG_CLASS(LLViewerTextureList);

public:
	static bool createUploadFile(const std::string& filename,
								 const std::string& out_filename, U8 codec);

	// WARNING: this method modifies the rawp image !
	static LLPointer<LLImageJ2C> convertToUploadFile(LLPointer<LLImageRaw> rawp,
													 S32 max_dimentions = -1,
													 bool force_lossless = false);

	static void processImageNotInDatabase(LLMessageSystem* msg, void** datap);
	static void receiveImageHeader(LLMessageSystem* msg, void** datap);
	static void receiveImagePacket(LLMessageSystem* msg, void** datap);

public:
	LLViewerTextureList();

	void init();
	void shutdown();
	void dump();
	LL_INLINE bool isInitialized() const		{ return mInitialized; }

	static void destroyGL(bool save_state = true);
	static void restoreGL();

	LLViewerFetchedTexture* findImage(const LLUUID& image_id);

	void dirtyImage(LLViewerFetchedTexture* image);

	// Using image stats, determine what images are necessary, and perform
	// image updates.
	void updateImages(F32 max_time);
	void forceImmediateUpdate(LLViewerFetchedTexture* imagep);

	// Decode and create textures for all images currently in list.
	S32 decodeAllImages(F32 max_decode_time);

	LL_INLINE S32 getMaxResidentTexMem() const	{ return mMaxResidentTexMemInMegaBytes; }
	LL_INLINE S32 getMaxTotalTextureMem() const	{ return mMaxTotalTextureMemInMegaBytes; }
	LL_INLINE S32 getNumImages()				{ return mImageList.size(); }

	void updateMaxResidentTexMem(S32 mem);

	void doPrefetchImages();

	void clearFetchingRequests();

	static S32 getMinVideoRamSetting();
	static S32 getMaxVideoRamSetting(bool get_recommended = false);

	static void resetFrameStats();

	LL_INLINE void flushOldImages()				{ mFlushOldImages = true; }

	void addImage(LLViewerFetchedTexture* image);
	void deleteImage(LLViewerFetchedTexture* image);

private:
	void updateImagesDecodePriorities();
	F32  updateImagesCreateTextures(F32 max_time);
	F32  updateImagesFetchTextures(F32 max_time);
	void updateImagesUpdateStats();

	void addImageToList(LLViewerFetchedTexture* image);
	void removeImageFromList(LLViewerFetchedTexture* image);

	LLViewerFetchedTexture* getImage(const LLUUID& image_id,
									 FTType f_type = FTT_DEFAULT,
									 bool usemipmap = true,
									 // Get the requested level immediately upon creation:
									 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
									 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
									 S32 internal_format = 0,
									 U32 primary_format = 0,
									 LLHost from_host = LLHost());

	LLViewerFetchedTexture* getImageFromFile(const std::string& filename,
											 bool usemipmap = true,
											 // Get the requested level immediately upon creation:
											 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
											 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
											 S32 internal_format = 0,
											 U32 primary_format = 0,
											 const LLUUID& force_id = LLUUID::null);

	LLViewerFetchedTexture* getImageFromUrl(const std::string& url,
											FTType f_type,
											bool usemipmap = true,
											// Get the requested level immediately upon creation:
											LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
											S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
											S32 internal_format = 0,
											U32 primary_format = 0,
											const LLUUID& force_id = LLUUID::null);

	LLViewerFetchedTexture* createImage(const LLUUID& image_id,
										FTType f_type, bool usemipmap = true,
										// Get the requested level immediately upon creation.
										LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
										S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
										S32 internal_format = 0,
										U32 primary_format = 0,
										LLHost from_host = LLHost());

	// Request image from a specific host, used for baked avatar textures.
	// Implemented in header in case someone changes default params above. JC
	LLViewerFetchedTexture* getImageFromHost(const LLUUID& image_id,
											 FTType f_type, LLHost host)
	{
		return getImage(image_id, f_type, true, LLGLTexture::BOOST_NONE,
						LLViewerTexture::LOD_TEXTURE, 0, 0, host);
	}

private:
	static S32			sNumImages;
	static S32			sUpdatedThisFrame;
	static void			(*sUUIDCallback)(void**, const LLUUID&);

	typedef std::map<LLUUID, LLPointer<LLViewerFetchedTexture> > uuid_map_t;
	uuid_map_t			mUUIDMap;

	// Simply holds on to LLViewerFetchedTexture references to stop them from
	// being purged too soon
	std::vector<LLPointer<LLViewerFetchedTexture> > mImagePreloads;

	typedef std::set<LLPointer<LLViewerFetchedTexture>,
					 LLViewerFetchedTexture::Compare> priority_list_t;
	priority_list_t		mImageList;

	LLUUID				mLastUpdateUUID;
	LLUUID				mLastFetchUUID;

	S32					mMaxResidentTexMemInMegaBytes;
	S32					mMaxTotalTextureMemInMegaBytes;

	// Texture fetching parameters, based on debug settings and possibly on
	// last TP/login time and camera speed.
	F32					mUpdateHighPriority;
	F32					mUpdateMaxMediumPriority;
	F32					mUpdateMinMediumPriority;

	F32					mLastGLImageCleaning;

	bool				mFlushOldImages;
	bool				mInitialized;

public:
	bool				mForceResetTextureStats;

	typedef safe_hset<LLPointer<LLViewerFetchedTexture> > image_list_t;
	image_list_t		mCreateTextureList;

	typedef safe_hset<LLPointer<LLViewerFetchedTexture> > callback_list_t;
	callback_list_t		mCallbackList;

	// Note: just raw pointers because they are never referenced, just compared
	// against
	typedef fast_hset<LLViewerFetchedTexture*> dirty_list_t;
	dirty_list_t		mDirtyTextureList;

	static LLStat		sNumImagesStat;
	static LLStat		sNumUpdatesStat;
	static LLStat		sNumRawImagesStat;
	static LLStat		sGLTexMemStat;
	static LLStat		sGLBoundMemStat;

	static U32			sTextureBits;
	static U32			sTexturePackets;
	static F32			sLastTeleportTime;	// In gFrameTimeSeconds seconds
	static F32			sFetchingBoostFactor;
};

class LLUIImageList : public LLImageProviderInterface,
					  public LLSingleton<LLUIImageList>
{
	friend class LLSingleton<LLUIImageList>;

protected:
	LOG_CLASS(LLUIImageList);

public:
	// LLImageProviderInterface
	LLUIImagePtr getUIImageByID(const LLUUID& id) override;
	LLUIImagePtr getUIImage(const std::string& name) override;

	void cleanUp() override;

	bool initFromFile();

	LLUIImagePtr preloadUIImage(const std::string& name,
								const std::string& filename, bool use_mips,
								const LLRect& scale_rect);

	static void onUIImageLoaded(bool success, LLViewerFetchedTexture* texp,
								LLImageRaw* imagep, LLImageRaw* aux_imagep,
								S32 discard_level, bool is_final,
								void* userdata);

private:
	LLUIImagePtr loadUIImageByName(const std::string& name,
								   const std::string& filename,
		                           bool use_mips = MIPMAP_NO,
								   const LLRect& scale_rect = LLRect::null);

	LLUIImagePtr loadUIImageByID(const LLUUID& id, bool use_mips = MIPMAP_NO,
								 const LLRect& scale_rect = LLRect::null);

	LLUIImagePtr loadUIImage(LLViewerFetchedTexture* imagep,
							 const std::string& name,
							 bool use_mips = MIPMAP_NO,
							 const LLRect& scale_rect = LLRect::null);

private:
	struct LLUIImageLoadData
	{
		std::string		mImageName;
		LLRect			mImageScaleRegion;
	};

	typedef fast_hmap<std::string, LLUIImagePtr> uuid_ui_image_map_t;
	uuid_ui_image_map_t	mUIImages;

	// Keep a copy of UI textures to prevent them to be deleted.
	// mImageGLp of each UI texture equals to some LLUIImage.mImage.
	typedef std::list<LLPointer<LLViewerFetchedTexture> > tex_list_t;
	tex_list_t			mUITextureList;
};

extern LLViewerTextureList	gTextureList;
extern LLViewerTexture*		gImgPixieSmall;

#endif
