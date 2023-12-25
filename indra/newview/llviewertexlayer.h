/**
 * @file llviewertexlayer.h
 * @brief Viewer texture layer classes. Used for avatars.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_VIEWER_TEXLAYER_H
#define LL_VIEWER_TEXLAYER_H

#include "llavatarappearance.h"
#include "llextendedstatus.h"
#include "lltexlayer.h"
#include "lluuid.h"

#include "lldynamictexture.h"

class LLVOAvatarSelf;
class LLViewerTexLayerSetBuffer;
struct LLBakedUploadData;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLViewerTexLayerSet
//
// An ordered set of texture layers that gets composited into a single texture.
// Only exists for llavatarappearanceself.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLViewerTexLayerSet final : public LLTexLayerSet
{
public:
	LLViewerTexLayerSet(LLAvatarAppearance* const appearance);

	LL_INLINE LLViewerTexLayerSet* asViewerTexLayerSet() override
	{
		return this;
	}

	void requestUpdate() override;

	void requestUpload();
	void cancelUpload();

	bool isLocalTextureDataAvailable();
	bool isLocalTextureDataFinal();

	void updateComposite();
	void createComposite() override;

	LL_INLINE void setUpdatesEnabled(bool b) 			{ mUpdatesEnabled = b; }
	LL_INLINE bool getUpdatesEnabled() const 			{ return mUpdatesEnabled; }

	LLVOAvatarSelf* getAvatar();
	const LLVOAvatarSelf* getAvatar() const;

	LLViewerTexLayerSetBuffer* getViewerComposite();

private:
	bool mUpdatesEnabled;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLViewerTexLayerSetBuffer
//
// The composite image that a LLViewerTexLayerSetBuffer writes to. Each
// LLViewerTexLayerSetBuffer has one.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLViewerTexLayerSetBuffer final : public LLTexLayerSetBuffer,
										public LLViewerDynamicTexture
{
protected:
	LOG_CLASS(LLViewerTexLayerSetBuffer);

public:
	LLViewerTexLayerSetBuffer(LLTexLayerSet* const owner,
							  S32 width, S32 height);
	~LLViewerTexLayerSetBuffer() override;

	LL_INLINE LLViewerTexLayerSetBuffer* asViewerTexLayerSetBuffer() override
	{
		return this;
	}

	S8 getType() const override;
	bool isInitialized() const;
	static void dumpTotalByteCount();

	LL_INLINE LLViewerTexLayerSet* getViewerTexLayerSet()
	{
		return mTexLayerSet ? mTexLayerSet->asViewerTexLayerSet() : NULL;
	}

	static void uploadBakedTextureCoro(const std::string& url, LLUUID vfile_id,
									   LLBakedUploadData* data);

	//--------------------------------------------------------------------
	// Dynamic Texture Interface
	//--------------------------------------------------------------------
	bool needsRender() override;

	//--------------------------------------------------------------------
	// Tex Layer Render
	//--------------------------------------------------------------------
private:
	void preRenderTexLayerSet() override;
	void midRenderTexLayerSet(bool success) override;
	void postRenderTexLayerSet(bool success) override;
	LL_INLINE S32 getCompositeOriginX() const override	{ return getOriginX(); }
	LL_INLINE S32 getCompositeOriginY() const override	{ return getOriginY(); }
	LL_INLINE S32 getCompositeWidth() const override	{ return getFullWidth(); }
	LL_INLINE S32 getCompositeHeight() const override	{ return getFullHeight(); }

protected:
	// Pass these along for tex layer rendering.
	LL_INLINE void preRender(bool) override				{ preRenderTexLayerSet(); }
	LL_INLINE void postRender(bool success) override	{ postRenderTexLayerSet(success); }
	LL_INLINE bool render() override					{ return renderTexLayerSet(); }

	//--------------------------------------------------------------------
	// Uploads
	//--------------------------------------------------------------------
public:
	void requestUpload();
	void cancelUpload();
	// We need to upload a new texture:
	LL_INLINE bool uploadNeeded() const					{ return mNeedsUpload; }
 	// We have started uploading a new texture and are awaiting the result
	LL_INLINE bool uploadInProgress() const				{ return mUploadID.notNull(); }
	// We are expecting a new texture to be uploaded at some point
	LL_INLINE bool uploadPending() const				{ return mUploadPending; }

	static void onTextureUploadComplete(const LLUUID& uuid, void* userdata,
										S32 result,
										LLExtStat s = LLExtStat::NONE);

protected:
	bool isReadyToUpload();
	void doUpload(); 				// Does a read back and upload.
	void conditionalRestartUploadTimer();

	//--------------------------------------------------------------------
	// Updates
	//--------------------------------------------------------------------
public:
	void requestUpdate();
	bool requestUpdateImmediate();

protected:
	bool isReadyToUpdate();
	void doUpdate();
	void restartUpdateTimer();

private:
	// The current upload process (null if none).
	LLUUID			mUploadID;

 	// Tracks time since upload was requested and performed.
	LLFrameTimer    mNeedsUploadTimer;

 	// Tracks time since last upload failure.
	LLFrameTimer	mUploadRetryTimer;

	// Tracks time since update was requested and performed.
	LLFrameTimer    mNeedsUpdateTimer;

	// Number of times we have locally updated with lowres version of our baked
	// textures
	U32				mNumLowresUpdates;

 	// Number of times we have sent a lowres version of our baked textures to
	// the server
	U32				mNumLowresUploads;

	// Number of consecutive upload failures
	S32				mUploadFailCount;

 	// Whether we have received back the new baked textures
	bool			mUploadPending;

	// Whether we need to send our baked textures to the server
	bool			mNeedsUpload;

	// Whether we need to locally update our baked textures
	bool			mNeedsUpdate;

	static S32		sGLByteCount;
};

#endif  // LL_VIEWER_TEXLAYER_H
