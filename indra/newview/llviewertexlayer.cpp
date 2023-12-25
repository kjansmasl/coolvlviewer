/**
 * @file llviewertexlayer.cpp
 * @brief Viewer texture layer. Used for avatars.
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

#include "llviewerprecompiledheaders.h"

#include "llviewertexlayer.h"

#include "imageids.h"
#include "llcorehttputil.h"
#include "llfilesystem.h"
#include "llimagej2c.h"
#include "llnotifications.h"
#include "llsdserialize.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llpipeline.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llvoavatarself.h"

using namespace LLAvatarAppearanceDefines;

static constexpr S32 BAKE_UPLOAD_ATTEMPTS = 7;
// Actual delay grows by power of 2 each attempt:
static constexpr F32 BAKE_UPLOAD_RETRY_DELAY = 2.f;

//-----------------------------------------------------------------------------
// LLBakedUploadData()
// Used by LLTexLayerSetBuffer for a callback.
//-----------------------------------------------------------------------------

struct LLBakedUploadData
{
	LLBakedUploadData(const LLVOAvatarSelf* avatar,
					  LLViewerTexLayerSet* layerset,
					  const LLUUID& id, bool highest_res)
	:	mAvatar(avatar),
		mTexLayerSet(layerset),
		mID(id),
		mStartTime(LLFrameTimer::getTotalTime()),	// Record starting time
		mIsHighestRes(highest_res)
	{
	}

	const LLUUID			mID;
	// Note: backlink only; do not use a LLPointer:
	const LLVOAvatarSelf*	mAvatar;
	LLViewerTexLayerSet*	mTexLayerSet;
	// For measuring baked texture upload time
   	const U64				mStartTime;
	// Whether this is a "final" bake, or intermediate low res
   	const bool				mIsHighestRes;
};

//-----------------------------------------------------------------------------
// LLViewerTexLayerSetBuffer
// The composite image that a LLViewerTexLayerSet writes to. Each
// LLViewerTexLayerSet has one.
//-----------------------------------------------------------------------------

//static
S32 LLViewerTexLayerSetBuffer::sGLByteCount = 0;

LLViewerTexLayerSetBuffer::LLViewerTexLayerSetBuffer(LLTexLayerSet* const owner,
													 S32 width, S32 height)
:	LLTexLayerSetBuffer(owner),
	LLViewerDynamicTexture(width, height, 4,
 						   // ORDER_LAST => must render these after the hints
						   // are created.
						   LLViewerDynamicTexture::ORDER_LAST, false),
	// Not used for any logic here, just to sync sending of updates:
	mUploadPending(false),
	mNeedsUpload(false),
	mNumLowresUploads(0),
	mUploadFailCount(0),
	mNeedsUpdate(true),
	mNumLowresUpdates(0)
{
	mImageGLp->setNeedsAlphaAndPickMask(false);
	sGLByteCount += getSize();
	mNeedsUploadTimer.start();
	mNeedsUpdateTimer.start();
}

LLViewerTexLayerSetBuffer::~LLViewerTexLayerSetBuffer()
{
	sGLByteCount -= getSize();
	destroyGLTexture();
	for (S32 order = 0; order < ORDER_COUNT; ++order)
	{
		// Will fail in all but one case:
		LLViewerDynamicTexture::sInstances[order].erase(this);
	}
}

//virtual
S8 LLViewerTexLayerSetBuffer::getType() const
{
	return LLViewerDynamicTexture::LL_TEX_LAYER_SET_BUFFER;
}

//static
void LLViewerTexLayerSetBuffer::dumpTotalByteCount()
{
	llinfos << "Composite System GL Buffers: " << sGLByteCount / 1024 << "KB"
			<< llendl;
}

void LLViewerTexLayerSetBuffer::requestUpdate()
{
	restartUpdateTimer();
	mNeedsUpdate = true;
	mNumLowresUpdates = 0;
	// If we are in the middle of uploading a baked texture, we do not care
	// about it any more. When it is downloaded, ignore it.
	mUploadID.setNull();
}

void LLViewerTexLayerSetBuffer::requestUpload()
{
	conditionalRestartUploadTimer();
	mNeedsUpload = true;
	mNumLowresUploads = 0;
	mUploadPending = true;
}

void LLViewerTexLayerSetBuffer::conditionalRestartUploadTimer()
{
	// If we requested a new upload but have not even uploaded a low res
	// version of our last upload request, then keep the timer ticking instead
	// of resetting it.
	if (mNeedsUpload && mNumLowresUploads == 0)
	{
		mNeedsUploadTimer.unpause();
	}
	else
	{
		mNeedsUploadTimer.reset();
		mNeedsUploadTimer.start();
	}
}

void LLViewerTexLayerSetBuffer::restartUpdateTimer()
{
	mNeedsUpdateTimer.reset();
	mNeedsUpdateTimer.start();
}

void LLViewerTexLayerSetBuffer::cancelUpload()
{
	mNeedsUpload = false;
	mUploadPending = false;
	mNeedsUploadTimer.pause();
	mUploadRetryTimer.reset();
}

//virtual
bool LLViewerTexLayerSetBuffer::needsRender()
{
	llassert(mTexLayerSet->getAvatarAppearance() == gAgentAvatarp);
	if (!isAgentAvatarValid()) return false;

	bool update_now = mNeedsUpdate && isReadyToUpdate();
	bool upload_now = mNeedsUpload && isReadyToUpload();

	// Do not render if we do not want to (or are not ready to) upload or
	// update
	if (!update_now && !upload_now)
	{
		return false;
	}

	// Do not render if we are animating our appearance.
	if (gAgentAvatarp->getIsAppearanceAnimating())
	{
		return false;
	}

	// Do not render if we are trying to create a skirt texture but are not
	// wearing a skirt.
	if (gAgentAvatarp->getBakedTE(getViewerTexLayerSet()) == TEX_SKIRT_BAKED &&
		!gAgentAvatarp->isWearingWearableType(LLWearableType::WT_SKIRT))
	{
		cancelUpload();
		return false;
	}

	// Render if we have at least minimal level of detail for each local
	// texture.
	return getViewerTexLayerSet()->isLocalTextureDataAvailable();
}

//virtual
void LLViewerTexLayerSetBuffer::preRenderTexLayerSet()
{
	LLTexLayerSetBuffer::preRenderTexLayerSet();
	// Keep depth buffer, we do not need to clear it
	LLViewerDynamicTexture::preRender(false);
}

//virtual
void LLViewerTexLayerSetBuffer::postRenderTexLayerSet(bool success)
{
	LLTexLayerSetBuffer::postRenderTexLayerSet(success);
	LLViewerDynamicTexture::postRender(success);
}

//virtual
void LLViewerTexLayerSetBuffer::midRenderTexLayerSet(bool success)
{
	// Do we need to upload, and do we have sufficient data to create an
	// uploadable composite ?
	// *TODO: When do we upload the texture if gAgent.mNumPendingQueries is
	// non-zero ?
	bool update_now = mNeedsUpdate && isReadyToUpdate();
	bool upload_now = mNeedsUpload && isReadyToUpload();
	if (upload_now)
	{
		if (success)
		{
			doUpload();
		}
		else
		{
			llinfos << "Failed attempt to bake "
					<< mTexLayerSet->getBodyRegionName() << llendl;
			mUploadPending = false;
		}
	}

	if (update_now)
	{
		doUpdate();
	}

	// *TODO: old logic does not check success before setGLTextureCreated
	// We have valid texture data now
	mImageGLp->setGLTextureCreated(true);
}

bool LLViewerTexLayerSetBuffer::isInitialized() const
{
	return mImageGLp.notNull() && mImageGLp->isGLTextureCreated();
}

bool LLViewerTexLayerSetBuffer::isReadyToUpload()
{
	if (!gAgentQueryManager.hasNoPendingQueries())
	{
		return false; // Cannot upload if there are pending queries.
	}
	if (!isAgentAvatarValid() || gAgentAvatarp->isEditingAppearance())
	{
		return false; // Do not upload if avatar is using composites.
	}

	LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
	if (layer_set->isLocalTextureDataFinal())
	{
		// If we requested an upload and have the final is LOD ready, upload
		// (or wait a while if this is a retry)
		return mUploadFailCount == 0 ||
			   mUploadRetryTimer.getElapsedTimeF32() >=
				BAKE_UPLOAD_RETRY_DELAY * (1 << (mUploadFailCount - 1));
	}

	// Upload if we have hit a timeout. Upload is a pretty expensive process so
	// we need to make sure we are not doing uploads too frequently.
	static LLCachedControl<U32> timeout(gSavedSettings,
										"AvatarBakedTextureUploadTimeout");
	if (!timeout)
	{
		return false;
	}

	// The timeout period increases exponentially between every lowres
	// upload in order to prevent spamming the server with frequent uploads.
	U32 threshold = timeout * (1 << mNumLowresUploads);

	// If we hit our timeout and have textures available at even lower
	// resolution, then upload.
	return layer_set->isLocalTextureDataAvailable() &&
		   mNeedsUploadTimer.getElapsedTimeF32() >= threshold;
}

bool LLViewerTexLayerSetBuffer::isReadyToUpdate()
{
	// If we requested an update and have the final LOD ready, then update.
	LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
	if (layer_set->isLocalTextureDataFinal())
	{
		return true;
	}

	// If we have not done an update yet, then just do one now regardless of
	// state of textures.
	if (mNumLowresUpdates == 0)
	{
		return true;
	}

	// Update if we have hit a timeout. Unlike for uploads, we can make this
	// timeout fairly small since render unnecessarily does not cost much.
	static LLCachedControl<U32> timeout(gSavedSettings,
										"AvatarBakedLocalTextureUpdateTimeout");
	if (!timeout)
	{
		return false;
	}

	// If we hit our timeout and have textures available at even lower
	// resolution, then update.
	return layer_set->isLocalTextureDataAvailable() &&
		   mNeedsUpdateTimer.getElapsedTimeF32() >= (F32)timeout;
}

bool LLViewerTexLayerSetBuffer::requestUpdateImmediate()
{
	mNeedsUpdate = true;
	bool result = false;
	if (needsRender())
	{
		preRender(false);
		result = render();
		postRender(result);
	}
	return result;
}

// If needed, create the baked texture and send it out to the server, then wait
// for it to come back so we can switch to using it.
void LLViewerTexLayerSetBuffer::doUpload()
{
	LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
	EBakedTextureIndex baked_idx = layer_set->getBakedTexIndex();
	bool visible = layer_set->isVisible();
	bool skip = (U8)baked_idx >= gAgent.mUploadedBakes;
	if (!visible || skip)
	{
		// Do not wait for any upload result: this bake is invisible anyway
		mUploadPending = mNeedsUpload = false;
		mNeedsUploadTimer.pause();
		// Set bake image as invisible
		layer_set->getAvatar()->setNewBakedTexture(baked_idx, IMG_INVISIBLE);
	}
	if (skip)
	{
		// Do not upload this bake
		return;
	}

	bool highest_lod = layer_set->isLocalTextureDataFinal();
	llinfos << "Uploading baked " << layer_set->getBodyRegionName()
			<< (highest_lod ? " (full res)" : "(low res)") << llendl;

	gViewerStats.incStat(LLViewerStats::ST_TEX_BAKES);

	// Do not need caches since we are baked now (note: we would not *really*
	// be baked until this image is sent to the server and the AvatarAppearance
	// message is received).
	layer_set->deleteCaches();

	// Get the COLOR information from our texture
	U8* baked_color_data = new U8[mFullWidth * mFullHeight * 4];
	glReadPixels(mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, GL_RGBA,
				 GL_UNSIGNED_BYTE, baked_color_data);
	stop_glerror();

	// Get the MASK information from our texture
	LLGLSUIDefault gls_ui;
	LLPointer<LLImageRaw> baked_mask_image = new LLImageRaw(mFullWidth,
															mFullHeight, 1);
	U8* baked_mask_data = baked_mask_image->getData();
	layer_set->gatherMorphMaskAlpha(baked_mask_data,
									mOrigin.mX, mOrigin.mY,
									mFullWidth, mFullHeight);

	// Create the baked image from our color and mask information
	constexpr S32 baked_image_components = 5; // red green blue [bump] clothing
	LLPointer<LLImageRaw> baked_image = new LLImageRaw(mFullWidth, mFullHeight,
													   baked_image_components);
	U8* baked_image_data = baked_image->getData();
	S32 i = 0;
	for (S32 u = 0; u < mFullWidth; ++u)
	{
		for (S32 v = 0; v < mFullHeight; ++v)
		{
			S32 k = 4 * i;
			S32 j = k + i; // 5 * i
			baked_image_data[j++] = baked_color_data[k++];
			baked_image_data[j++] = baked_color_data[k++];
			baked_image_data[j++] = baked_color_data[k++];
			// Alpha should be correct for eyelashes:
			baked_image_data[j++] = baked_color_data[k];
			baked_image_data[j] = baked_mask_data[i++];
		}
	}

	LLPointer<LLImageJ2C> j2c_img = new LLImageJ2C;
	// Writes into baked_color_data. 5 channels (RGB, heightfield/alpha, mask)
	if (j2c_img->encode(baked_image, "LL_RGBHM"))
	{
		LLTransactionID tid;
		tid.generate();
		const LLAssetID asset_id =
			tid.makeAssetID(gAgent.getSecureSessionID());
		LLFileSystem j2c_file(asset_id, LLFileSystem::OVERWRITE);
		if (j2c_file.write(j2c_img->getData(), j2c_img->getDataSize()))
		{
			// Read back the file and validate.
			bool valid = false;
			LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
			S32 file_size = 0;
			LLFileSystem file(asset_id);
			file_size = file.getSize();
			// Data buffer MUST be allocated using LLImageBase
			U8* data = integrity_test->allocateData(file_size);
			if (data)
			{
				file.read(data, file_size);
				if (data)
				{
					// integrity_test will delete 'data'
					valid = integrity_test->validate(data, file_size);
				}
				else
				{
					integrity_test->setLastError("Unable to read entire file");
				}
			}
			else
			{
				integrity_test->setLastError("Unable to allocate memory");
			}

			if (valid)
			{
				// Baked_upload_data is owned by the responder and deleted
				// after the request completes.
				LLBakedUploadData* baked_upload_data =
					new LLBakedUploadData(gAgentAvatarp, layer_set, asset_id,
										  highest_lod);
				// Upload ID is used to avoid overlaps, e.g. when the user
				// rapidly makes two changes outside of Face Edit.
				mUploadID = asset_id;

				// Upload the image

				static LLCachedControl<bool> use_udp(gSavedSettings,
													 "BakedTexUploadForceUDP");
				const std::string& url =
					gAgent.getRegionCapability("UploadBakedTexture");
				if (!url.empty() && !use_udp &&
					// Try last ditch attempt via asset store if cap upload is
					// failing
					mUploadFailCount < BAKE_UPLOAD_ATTEMPTS - 1)
				{
					llinfos << "Baked texture upload via capability of "
							<< mUploadID << " to " << url << llendl;
					gCoros.launch("uploadBakedTextureCoro",
								  boost::bind(&LLViewerTexLayerSetBuffer::uploadBakedTextureCoro,
											  url, mUploadID,
											  baked_upload_data));
				}
				else if (gAssetStoragep)
				{
					gAssetStoragep->storeAssetData(tid,
												   LLAssetType::AT_TEXTURE,
												   onTextureUploadComplete,
												   baked_upload_data,
												   true,	// temp_file
												   true,	// is_priority
												   true);	// store_local
					llinfos << "Baked texture upload via Asset Store."
							<< llendl;
				}

				if (highest_lod)
				{
					// Sending the final LOD for the baked texture. All done,
					// pause the upload timer so we know how long it took.
					mNeedsUpload = false;
					mNeedsUploadTimer.pause();
				}
				else
				{
					// Sending a lower level LOD for the baked texture. Restart
					// the upload timer.
					++mNumLowresUploads;
					mNeedsUploadTimer.unpause();
					mNeedsUploadTimer.reset();
				}
			}
			else
			{
				// The read back and validate operation failed. Remove the
				// uploaded file.
				mUploadPending = false;
				LLFileSystem::removeFile(asset_id);
				llinfos << "Unable to create baked upload file (reason: corrupted)."
						<< llendl;
			}
		}
	}
	else
	{
		// The cache write file operation failed.
		mUploadPending = false;
		llinfos << "Unable to create baked upload file (reason: failed to write file)"
				<< llendl;
	}

	delete[] baked_color_data;
}

void upload_failure(const LLUUID& vfile_id, const std::string& reason)
{
	LLSD args;
	args["FILE"] = vfile_id.asString();
	args["REASON"] = reason;
	gNotifications.add("CannotUploadReason", args);
}

//static
void LLViewerTexLayerSetBuffer::uploadBakedTextureCoro(const std::string& url,
													   LLUUID vfile_id,
													   LLBakedUploadData* data)
{
	if (!data)
	{
		llwarns << "No baked upload data for baked teture " << vfile_id
				<< ". Baked texture upload aborted." << llendl;
		return;
	}

	if (!LLFileSystem::getExists(vfile_id))
	{
		llwarns << "Non-existent cache file Id: " << vfile_id
				<< ". Baked texture upload aborted." << llendl;
		delete data;
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("uploadBakedTextureCoro");
	LLSD result = adapter.postAndSuspend(url, LLSD());

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		upload_failure(vfile_id, status.toString());
		delete data;
		return;
	}
	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);

	if (!result.has("state"))
	{
		llwarns << "Error: " << result << llendl;
		upload_failure(vfile_id, "malformed response contents.");
		delete data;
		return;
	}

	std::string state = result["state"].asString();
	if (state != "upload")
	{
		llwarns << "Error: " << result << llendl;
		std::string message = result["message"].asString();
		if (message.empty())
		{
			message = "unexpected state in response: " + state;
		}
		upload_failure(vfile_id, message);
		delete data;
		return;
	}

	std::string uploader = result["uploader"].asString();
	if (uploader.empty())
	{
		llwarns << "Error: " << result << llendl;
		upload_failure(vfile_id, "no uploader URL in response.");
		delete data;
		return;
	}

	result = adapter.postFileAndSuspend(uploader, vfile_id,
										LLAssetType::AT_TEXTURE);
	status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		upload_failure(vfile_id, status.toString());
		delete data;
		return;
	}
	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);

	state = result["state"].asString();
	if (state != "complete")
	{
		llwarns << "Error: " << result << llendl;
		std::string message = result["message"].asString();
		if (message.empty())
		{
			message = "unexpected state in response: " + state;
		}
		upload_failure(vfile_id, message);
		delete data;
		return;
	}

	LLUUID new_id = result["new_asset"].asUUID();
	if (new_id.isNull())
	{
		llwarns << "Error: " << result << llendl;
		upload_failure(vfile_id, "missing new asset Id in response.");
		delete data;
		return;
	}

	// Rename the file in the cache to the actual asset id
	LLFileSystem::renameFile(vfile_id, new_id);

	state = result["state"].asString();
	llinfos << "Result: " << state << " - New Id: " << new_id << llendl;
	S32 success = state == "complete" ? 0 : -1;
	// Note: the baked upload data will be deleted by the following call
	onTextureUploadComplete(new_id, data, success);
}

// Mostly bookkeeping; don't need to actually "do" anything since render() will
// actually do the update.
void LLViewerTexLayerSetBuffer::doUpdate()
{
	LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
	if (layer_set->isLocalTextureDataFinal())
	{
		mNeedsUpdate = false;
	}
	else
	{
		++mNumLowresUpdates;
	}

	restartUpdateTimer();

	// need to swtich to using this layerset if this is the first update after
	// getting the lowest LOD
	layer_set->getAvatar()->updateMeshTextures();
}

// StoreAssetData callback (not fixed)
//static
void LLViewerTexLayerSetBuffer::onTextureUploadComplete(const LLUUID& uuid,
														void* userdata,
														S32 result, LLExtStat)
{
	if (!userdata) return;

	LLBakedUploadData* baked_upload_data = (LLBakedUploadData*)userdata;

	if (isAgentAvatarValid() &&
		// Sanity check: only the user's avatar should be uploading textures:
		baked_upload_data->mAvatar == gAgentAvatarp &&
		baked_upload_data->mTexLayerSet->hasComposite())
	{
		LLViewerTexLayerSetBuffer* layerset_buffer =
			baked_upload_data->mTexLayerSet->getViewerComposite();
		S32 failures = layerset_buffer->mUploadFailCount;
		layerset_buffer->mUploadFailCount = 0;

		if (layerset_buffer->mUploadID.isNull())
		{
			// The upload got cancelled, we should be in the process of baking
			// a new texture so request an upload with the new data.
			// BAP: does this really belong in this callback, as opposed to
			// where the cancellation takes place ? Suspect this does nothing.
			layerset_buffer->requestUpload();
		}
		else if (baked_upload_data->mID == layerset_buffer->mUploadID)
		{
			// This is the upload we are currently waiting for.
			layerset_buffer->mUploadID.setNull();
			const std::string name =
				baked_upload_data->mTexLayerSet->getBodyRegionName();
			const std::string resolution =
				baked_upload_data->mIsHighestRes ? " full res " : " low res ";
			if (result >= 0)
			{
				// Allows sending of AgentSetAppearance later:
				layerset_buffer->mUploadPending = false;
				ETextureIndex baked_te =
					gAgentAvatarp->getBakedTE(layerset_buffer->getViewerTexLayerSet());
				// Update baked texture info with the new UUID
				U64 now = LLFrameTimer::getTotalTime();		// Record starting time
				llinfos << "Baked" << resolution << "texture upload for "
						<< name << " took "
						<< (S32)((now - baked_upload_data->mStartTime) / 1000)
						<< " ms" << llendl;
				gAgentAvatarp->setNewBakedTexture(baked_te, uuid);
			}
			else
			{
				++failures;
				S32 max_attempts =
					baked_upload_data->mIsHighestRes ? BAKE_UPLOAD_ATTEMPTS
													 : 1; // only retry final bakes
				llwarns << "Baked" << resolution << "texture upload for "
						<< name << " failed (attempt " << failures << "/"
						<< max_attempts << ")" << llendl;
				if (failures < max_attempts)
				{
					layerset_buffer->mUploadFailCount = failures;
					layerset_buffer->mUploadRetryTimer.start();
					layerset_buffer->requestUpload();
				}
			}
		}
		else
		{
			llinfos << "Received baked texture out of date, ignored."
					<< llendl;
		}

		gAgentAvatarp->dirtyMesh();
	}
	else
	{
		// Baked texture failed to upload (in which case since we didn't set
		// the new baked texture, it means that they'll try and rebake it at
		// some point in the future (after login ?)), or this response to
		// upload is out of date, in which case a current response should be on
		// the way or already processed.
		llwarns << "Baked upload failed" << llendl;
	}

	delete baked_upload_data;
}

//-----------------------------------------------------------------------------
// LLViewerTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLViewerTexLayerSet::LLViewerTexLayerSet(LLAvatarAppearance* const appearance)
:	LLTexLayerSet(appearance),
	mUpdatesEnabled(false)
{
}

// Returns true if at least one packet of data has been received for each of
// the textures that this layerset depends on.
bool LLViewerTexLayerSet::isLocalTextureDataAvailable()
{
	return mAvatarAppearance->isSelf() &&
		   getAvatar()->isLocalTextureDataAvailable(this);
}

// Returns true if all of the data for the textures that this layerset depends
// on have arrived.
bool LLViewerTexLayerSet::isLocalTextureDataFinal()
{
	return mAvatarAppearance->isSelf() &&
		   getAvatar()->isLocalTextureDataFinal(this);
}

void LLViewerTexLayerSet::requestUpdate()
{
	if (mUpdatesEnabled)
	{
		createComposite();
		getViewerComposite()->requestUpdate();
	}
}

void LLViewerTexLayerSet::requestUpload()
{
	createComposite();
	getViewerComposite()->requestUpload();
}

void LLViewerTexLayerSet::cancelUpload()
{
	LLViewerTexLayerSetBuffer* composite = getViewerComposite();
	if (composite)
	{
		composite->cancelUpload();
	}
}

void LLViewerTexLayerSet::updateComposite()
{
	createComposite();
	getViewerComposite()->requestUpdateImmediate();
}

//virtual
void LLViewerTexLayerSet::createComposite()
{
	if (!mComposite)
	{
		S32 width = mInfo->getWidth();
		S32 height = mInfo->getHeight();
		if (!mAvatarAppearance->isSelf())
		{
			llerrs << "composites should not be created for non-self avatars !"
				   << llendl;
		}
		mComposite = new LLViewerTexLayerSetBuffer(this, width, height);
	}
}

LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar()
{
	// Note: this is a legit static cast, because LLAvatarAppearance is only
	// used as a parent class for LLVOAvatar: should this change in the future,
	// the cast below would become illegal.
	LLVOAvatar* avatarp = (LLVOAvatar*)mAvatarAppearance;
	return avatarp->isSelf() ? (LLVOAvatarSelf*)avatarp : NULL;
}

const LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar() const
{
	// Note: this is a legit static cast, because LLAvatarAppearance is only
	// used as a parent class for LLVOAvatar: should this change in the future,
	// the cast below would become illegal.
	const LLVOAvatar* avatarp = (const LLVOAvatar*)mAvatarAppearance;
	return avatarp->isSelf() ? (const LLVOAvatarSelf*)avatarp : NULL;
}

LLViewerTexLayerSetBuffer* LLViewerTexLayerSet::getViewerComposite()
{
	LLTexLayerSetBuffer* bufferp = getComposite();
	return bufferp ? bufferp->asViewerTexLayerSetBuffer() : NULL;
}
