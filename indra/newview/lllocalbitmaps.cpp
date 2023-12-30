/**
 * @file lllocalbitmaps.cpp
 * @author Vaalith Jinn, code cleanup by Henri Beauchamp
 * @brief Local Bitmaps source
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Linden Research, Inc.
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

#include <time.h>

#include "lllocalbitmaps.h"

#include "imageids.h"
#include "lldir.h"
#include "llimagebmp.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "lllocaltextureobject.h"
#include "llnotifications.h"

#include "llagentwearables.h"
#include "llface.h"
#include "llfetchedgltfmaterial.h"
#include "llmaterialmgr.h"
#include "llviewerobjectlist.h"
#include "llviewertexturelist.h"
#include "llviewerwearable.h"
#include "llvoavatarself.h"
#include "llvovolume.h"

S32 LLLocalBitmap::sBitmapsListVersion = 0;
bool LLLocalBitmap::sNeedsRebake = false;
std::list<LLLocalBitmap*> LLLocalBitmap::sBitmapList;
LLLocalBitmapTimer LLLocalBitmap::sTimer;

constexpr F32 LL_LOCAL_TIMER_HEARTBEAT = 3.f;
constexpr S32 LL_LOCAL_UPDATE_RETRIES  = 5;

///////////////////////////////////////////////////////////////////////////////
// LLLocalBitmap class
///////////////////////////////////////////////////////////////////////////////

LLLocalBitmap::LLLocalBitmap(std::string filename)
:	mFilename(filename),
	mShortName(gDirUtilp->getBaseFileName(filename, true)),
	mLastModified(0),
	mValid(false),
	mLinkStatus(LS_ON),
	mUpdateRetries(LL_LOCAL_UPDATE_RETRIES)
{
	mTrackingID.generate();

	/* extension */
	std::string temp_exten = gDirUtilp->getExtension(mFilename);
	if (temp_exten == "bmp")
	{
		mExtension = ET_IMG_BMP;
	}
	else if (temp_exten == "tga")
	{
		mExtension = ET_IMG_TGA;
	}
	else if (temp_exten == "jpg" || temp_exten == "jpeg")
	{
		mExtension = ET_IMG_JPG;
	}
	else if (temp_exten == "png")
	{
		mExtension = ET_IMG_PNG;
	}
	else
	{
		llwarns << "File of no valid extension given, local bitmap creation aborted. Filename: "
				<< mFilename << llendl;
		return; // no valid extension.
	}

	// Next phase of unit creation is nearly the same as an update cycle. We
	// are running updateSelf as a special case with the optional UT_FIRSTUSE
	// which omits the parts associated with removing the outdated texture.
	mValid = updateSelf(UT_FIRSTUSE);
}

LLLocalBitmap::~LLLocalBitmap()
{
	// Replace IDs with defaults
	if (mValid && isAgentAvatarValid())
	{
		replaceIDs(mWorldID, IMG_DEFAULT);
		LLLocalBitmap::doRebake();
	}

	for (U32 i = 0, count = mGLTFMaterialWithLocalTextures.size(); i < count;
		 ++i)
	{
		LLGLTFMaterial* matp = mGLTFMaterialWithLocalTextures[i].get();
		if (matp)
		{
			matp->removeLocalTextureTracking(mTrackingID);
		}
	}
	mChangedSignal(mTrackingID, mWorldID, LLUUID::null);
	mChangedSignal.disconnect_all_slots();

	// Delete self from the textures list
	LLViewerFetchedTexture* texp = gTextureList.findImage(mWorldID);
	if (texp)
	{
		gTextureList.deleteImage(texp);
		texp->unref();
	}
}

//static
void LLLocalBitmap::cleanupClass()
{
	std::for_each(sBitmapList.begin(), sBitmapList.end(), DeletePointer());
	sBitmapList.clear();
}

bool LLLocalBitmap::updateSelf(EUpdateType optional_firstupdate)
{
	if (mLinkStatus != LS_ON)
	{
		return false;
	}
	if (!LLFile::exists(mFilename))
	{
		mLinkStatus = LS_BROKEN;
		LLSD args;
		args["FNAME"] = mFilename;
		gNotifications.add("LocalBitmapsUpdateFileNotFound", args);
		return false;
	}

	// Verifying that the file has indeed been modified
	time_t new_last_modified = LLFile::lastModidied(mFilename);
	if (mLastModified == new_last_modified)
	{
		return false;
	}

	// Loading the image file and decoding it; here is a critical point which,
	// if fails, invalidates the whole update (or unit creation) process.
	LLPointer<LLImageRaw> raw_image = new LLImageRaw();
	if (decodeBitmap(raw_image))
	{
		// Decode is successful, we can safely proceed.
		LLUUID old_id;
		if (optional_firstupdate != UT_FIRSTUSE && mWorldID.notNull())
		{
			old_id = mWorldID;
		}
		mWorldID.generate();
		mLastModified = new_last_modified;

		LLPointer<LLViewerFetchedTexture> texp =
			new LLViewerFetchedTexture("file://" + mFilename, FTT_LOCAL_FILE,
									   mWorldID, true);
		texp->createGLTexture(0, raw_image);
		texp->setCachedRawImage(0, raw_image);
		texp->ref();
		gTextureList.addImage(texp);

		if (optional_firstupdate != UT_FIRSTUSE)
		{
			// Seek out everything old_id uses and replace it with mWorldID
			replaceIDs(old_id, mWorldID);

			// Remove old_id from gimagelist
			LLViewerFetchedTexture* oldtexp = gTextureList.findImage(old_id);
			if (oldtexp)
			{
				gTextureList.deleteImage(oldtexp);
				oldtexp->unref();
			}
			else
			{
				llwarns_once << "Could not find texture for id: " << old_id
							 << llendl;
			}
		}

		mUpdateRetries = LL_LOCAL_UPDATE_RETRIES;
		return true;
	}

	// If decoding failed, we get here and it will attempt to decode it in the
	// next cycles until mUpdateRetries runs out. this is done because some
	// software lock the bitmap while writing to it.
	if (mUpdateRetries)
	{
		--mUpdateRetries;
	}
	else
	{
		mLinkStatus = LS_BROKEN;
		LLSD args;
		args["FNAME"] = mFilename;
		args["NRETRIES"] = LL_LOCAL_UPDATE_RETRIES;
		gNotifications.add("LocalBitmapsUpdateFailedFinal", args);
	}
	return false;
}

boost::signals2::connection LLLocalBitmap::setChangedCallback(const changed_cb_t& cb)
{
	return mChangedSignal.connect(cb);
}

void LLLocalBitmap::addGLTFMaterial(LLGLTFMaterial* new_matp)
{
	if (!new_matp)
	{
		return;
	}

	for (U32 i = 0, count = mGLTFMaterialWithLocalTextures.size(); i < count; )
	{
		const LLPointer<LLGLTFMaterial>& matp =
			mGLTFMaterialWithLocalTextures[i];
		if (matp.get() == new_matp)
		{
			return;
		}
		if (matp.isNull() || matp->getNumRefs() == 1)
		{
			// This material is no more in use by anyone else: remove it.
			if (i != --count)
			{
				mGLTFMaterialWithLocalTextures[i] =
					std::move(mGLTFMaterialWithLocalTextures.back());
			}
			mGLTFMaterialWithLocalTextures.pop_back();
			continue;
		}
		++i;
	}

	new_matp->addLocalTextureTracking(mTrackingID, mWorldID);
	mGLTFMaterialWithLocalTextures.emplace_back(new_matp);
}

bool LLLocalBitmap::decodeBitmap(LLPointer<LLImageRaw> rawimg)
{
	bool decode_successful = false;

	switch (mExtension)
	{
		case ET_IMG_BMP:
		{
			LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
			if (bmp_image->load(mFilename) && bmp_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_TGA:
		{
			LLPointer<LLImageTGA> tga_image = new LLImageTGA;
			if (tga_image->load(mFilename) && tga_image->decode(rawimg) &&
				(tga_image->getComponents() == 3 ||
				 tga_image->getComponents() == 4))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_JPG:
		{
			LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
			if (jpeg_image->load(mFilename) &&
				jpeg_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_PNG:
		{
			LLPointer<LLImagePNG> png_image = new LLImagePNG;
			if (png_image->load(mFilename) &&
				png_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		default:
		{
			// separating this into -several- llwarns calls because in the
			// extremely unlikely case that this happens, accessing mFilename
			// and any other object properties might very well crash the
			// viewer. Getting here should be impossible, or there's been a
			// pretty serious bug.

			llwarns << "During a decode attempt, the following local bitmap had no properly assigned extension: "
					<< mFilename
					<< ". Disabling further update attempts for this file."
					<< llendl;
			mLinkStatus = LS_BROKEN;
		}
	}

	return decode_successful;
}

void LLLocalBitmap::replaceIDs(const LLUUID& old_id, LLUUID new_id)
{
	// Checking for misuse.
	if (old_id == new_id)
	{
		llinfos << "An attempt was made to replace a texture with itself (matching UUIDs): "
				<< old_id.asString() << llendl;
		return;
	}

	mChangedSignal(mTrackingID, old_id, new_id);

	// Processing updates per channel; makes the process scalable. The only
	// actual difference is in SetTE* call i.e. SetTETexture, SetTENormal, etc.
	updateUserPrims(old_id, new_id, LLRender::DIFFUSE_MAP);
	updateUserPrims(old_id, new_id, LLRender::NORMAL_MAP);
	updateUserPrims(old_id, new_id, LLRender::SPECULAR_MAP);

	updateUserVolumes(old_id, new_id, LLRender::LIGHT_TEX);
	// Is not there supposed to be an IMG_DEFAULT_SCULPT or something ?
	updateUserVolumes(old_id, new_id, LLRender::SCULPT_TEX);

	// Default safeguard image for layers
	if (new_id == IMG_DEFAULT)
	{
		new_id = IMG_DEFAULT_AVATAR;
	}

	// It does not actually update all of those, it merely checks if any of
	// them contains the referenced ID and if so, updates.
	updateUserLayers(old_id, new_id, LLWearableType::WT_ALPHA);
	updateUserLayers(old_id, new_id, LLWearableType::WT_EYES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_GLOVES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_JACKET);
	updateUserLayers(old_id, new_id, LLWearableType::WT_PANTS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SHIRT);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SHOES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SKIN);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SKIRT);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SOCKS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_TATTOO);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNIVERSAL);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNDERPANTS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNDERSHIRT);

	updateGLTFMaterials(old_id, new_id);
}

// This function sorts the faces from a getFaceList[getNumFaces] into a list of
// objects in order to prevent multiple sendTEUpdate calls per object during
// updateUserPrims.
void LLLocalBitmap::prepUpdateObjects(const LLUUID& old_id, U32 channel,
									  std::vector<LLViewerObject*>& obj_list)
{
	LLViewerFetchedTexture* oldtextp = gTextureList.findImage(old_id);
	if (!oldtextp)
	{
		llwarns_once << "Could not find texture for id: " << old_id << llendl;
		return;
	}

	U32 count = oldtextp->getNumFaces(channel);
	obj_list.reserve(count);
	for (U32 i = 0; i < count; ++i)
	{
		// Getting an object from a face
		LLFace* facep = (*oldtextp->getFaceList(channel))[i];
		if (facep)
		{
			LLViewerObject* objectp = facep->getViewerObject();
			if (objectp)
			{
				// We have an object, we'll take its UUID and compare it to
				// whatever we already have in the returnable object list. If
				// there is a match, we do not add it, to prevent duplicates.
				const LLUUID& mainlist_obj_id = objectp->getID();

				// Look for duplicates
				bool add_object = true;
				for (U32 j = 0, count2 = obj_list.size(); j < count2; ++j)
				{
					LLViewerObject* objp = obj_list[j];
					if (objp->getID() == mainlist_obj_id)
					{
						add_object = false; // Duplicate found.
						break;
					}
				}

				if (add_object)
				{
					obj_list.push_back(objectp);
				}
			}
		}
	}
}

void LLLocalBitmap::updateUserPrims(const LLUUID& old_id, const LLUUID& new_id,
									U32 channel)
{
	std::vector<LLViewerObject*> objectlist;
	prepUpdateObjects(old_id, channel, objectlist);

	for (U32 i = 0, count = objectlist.size(); i < count; ++i)
	{
		LLViewerObject* objectp = objectlist[i];
		LLDrawable* drawablep = objectp->mDrawable;
		if (!drawablep)
		{
			continue;
		}

		bool update_tex = false;
		bool update_mat = false;

		for (U8 te = 0, faces = objectp->getNumFaces(); te < faces; ++te)
		{
			LLFace* facep = drawablep->getFace(te);
			if (facep && facep->getTexture(channel) &&
				facep->getTexture(channel)->getID() == old_id)
			{
				switch (channel)
				{
					case LLRender::DIFFUSE_MAP:
						objectp->setTETexture(te, new_id);
						update_tex = true;
						break;

					case LLRender::NORMAL_MAP:
						objectp->setTENormalMap(te, new_id);
						update_mat = update_tex = true;
						break;

					case LLRender::SPECULAR_MAP:
						objectp->setTESpecularMap(te, new_id);
						update_mat = update_tex = true;
				}
			}
		}

		if (update_tex)
		{
			objectp->sendTEUpdate();
		}
		if (update_mat && drawablep->getVOVolume())
		{
			drawablep->getVOVolume()->faceMappingChanged();
		}
	}
}

void LLLocalBitmap::updateUserVolumes(const LLUUID& old_id,
									  const LLUUID& new_id, U32 channel)
{
	LLViewerFetchedTexture* oldtextp = gTextureList.findImage(old_id);
	if (!oldtextp)
	{
		llwarns_once << "Could not find texture for id: " << old_id << llendl;
		return;
	}
	if (channel != LLRender::LIGHT_TEX && channel != LLRender::SCULPT_TEX)
	{
		llwarns_once << "Bad texture channel: " << channel << llendl;
		llassert(false);
		return;
	}

	for (U32 i = 0, count = oldtextp->getNumVolumes(channel); i < count; ++i)
	{
		LLVOVolume* vovolp = (*oldtextp->getVolumeList(channel))[i];
		if (!vovolp) continue;	// Paranoia

		if (channel == LLRender::LIGHT_TEX)
		{
			if (vovolp->getLightTextureID() == old_id)
			{
				vovolp->setLightTextureID(new_id);
			}
		}
		else
		{
			LLViewerObject* objectp = (LLViewerObject*)vovolp;
			if (objectp && objectp->isSculpted() && objectp->getVolume() &&
				objectp->getVolume()->getParams().getSculptID() == old_id)
			{
				const LLSculptParams* paramsp = objectp->getSculptParams();
				if (!paramsp) continue;

				LLSculptParams new_params(*paramsp);
				new_params.setSculptTexture(new_id, paramsp->getSculptType());
				objectp->setParameterEntry(LLNetworkData::PARAMS_SCULPT,
										   new_params, true);
			}
		}
	}
}

void LLLocalBitmap::updateUserLayers(const LLUUID& old_id,
									 const LLUUID& new_id,
									 LLWearableType::EType type)
{
	std::vector<LLLocalTextureObject*> texture_list;
	for (U32 i = 0, count = gAgentWearables.getWearableCount(type); i < count;
		 ++i)
	{
		LLViewerWearable* vwp = gAgentWearables.getViewerWearable(type, i);
		if (!vwp)
		{
			continue;
		}
		vwp->getLocalTextureListSeq(texture_list);
		for (U32 j = 0, count2 = texture_list.size(); j < count2; ++j)
		{
			LLLocalTextureObject* ltop = texture_list[i];
			if (!ltop || ltop->getID() != old_id)
			{
				continue;
			}
			// Cannot keep that as static const, gives errors, so leaving this
			// var here:
			U32 lti = 0;
			auto baked_texind =
				ltop->getTexLayer(lti)->getTexLayerSet()->getBakedTexIndex();

			auto reg_texind = getTexIndex(type, baked_texind);
			if (reg_texind == LLAvatarAppearanceDefines::TEX_NUM_INDICES)
			{
				continue;
			}
			U32 index;
			if (gAgentWearables.getWearableIndex(vwp, index))
			{
				gAgentAvatarp->setLocalTexture(reg_texind,
											   gTextureList.getImage(new_id),
											   false, index);
				gAgentAvatarp->wearableUpdated(type, false);

				// Flag for rebake once this update cycle is finished.
				LLLocalBitmap::setNeedsRebake();
			}
		}
	}
}

void LLLocalBitmap::updateGLTFMaterials(const LLUUID& old_id,
										const LLUUID& new_id)
{
	for (U32 i = 0, count = mGLTFMaterialWithLocalTextures.size(); i < count; )
	{
		LLPointer<LLGLTFMaterial>& matp = mGLTFMaterialWithLocalTextures[i];
		if (matp.notNull() && matp->getNumRefs() > 1 &&
			matp->replaceLocalTexture(mTrackingID, old_id, new_id))
		{
			++i;
		}
		// Matching Id not found or material no more in use: remove it.
		if (i != --count)
		{
			mGLTFMaterialWithLocalTextures[i] =
				std::move(mGLTFMaterialWithLocalTextures.back());
		}
		mGLTFMaterialWithLocalTextures.pop_back();
	}

	// Render material consists of base and override materials, make sure
	// replaceLocalTexture() gets called for base and override before
	// applyOverride().
	typedef LLFetchedGLTFMaterial::te_list_t entries_list_t;
	for (U32 i = 0, count = mGLTFMaterialWithLocalTextures.size(); i < count;
		 ++i)
	{
		LLFetchedGLTFMaterial* matp =
			mGLTFMaterialWithLocalTextures[i]->asFetched();
		if (!matp)
		{
			continue;
		}
		// Normally a change in applied material id is supposed to drop
		// overrides thus reset material, but local materials currently reuse
		// their existing asset Id, since their purpose is to preview how
		// material will work in-world, overrides included, so do an override
		// to render update instead.
		const entries_list_t& entries = matp->getTexEntries();
		for (entries_list_t::const_iterator it = entries.begin(),
											end = entries.end();
			 it != end; ++it)
		{
			LLTextureEntry* tep = *it;
			if (!tep)	// Paranoia
			{
				continue;
			}

			LLGLTFMaterial* omatp = tep->getGLTFMaterialOverride();
			if (!omatp || !tep->getGLTFRenderMaterial())
			{
				continue;
			}

			// Do not create a new material, reuse existing pointer.
			
			LLFetchedGLTFMaterial* rmatp =
				tep->getGLTFRenderMaterial()->asFetched();
			if (rmatp)
			{
				*rmatp = *matp;
				rmatp->applyOverride(*omatp);
			}
		}
	}	
}

LLAvatarAppearanceDefines::ETextureIndex LLLocalBitmap::getTexIndex(LLWearableType::EType type,
																	LLAvatarAppearanceDefines::EBakedTextureIndex baked_texind)
{
	// Using TEX_NUM_INDICES as a default/fail return
	LLAvatarAppearanceDefines::ETextureIndex result = LLAvatarAppearanceDefines::TEX_NUM_INDICES;

	switch (type)
	{
		case LLWearableType::WT_ALPHA:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_EYES:
					result = LLAvatarAppearanceDefines::TEX_EYES_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_HAIR:
					result = LLAvatarAppearanceDefines::TEX_HAIR_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_ALPHA;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_EYES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_EYES)
			{
				result = LLAvatarAppearanceDefines::TEX_EYES_IRIS;
			}

			break;
		}

		case LLWearableType::WT_GLOVES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_GLOVES;
			}

			break;
		}

		case LLWearableType::WT_JACKET:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_JACKET;
			}
			else if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_JACKET;
			}

			break;
		}

		case LLWearableType::WT_PANTS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_PANTS;
			}

			break;
		}

		case LLWearableType::WT_SHIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_SHIRT;
			}

			break;
		}

		case LLWearableType::WT_SHOES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_SHOES;
			}

			break;
		}

		case LLWearableType::WT_SKIN:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_BODYPAINT;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_BODYPAINT;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_BODYPAINT;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_SKIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_SKIRT)
			{
				result = LLAvatarAppearanceDefines::TEX_SKIRT;
			}

			break;
		}

		case LLWearableType::WT_SOCKS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_SOCKS;
			}

			break;
		}

		case LLWearableType::WT_TATTOO:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_TATTOO;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_UNIVERSAL:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_HAIR:
					result = LLAvatarAppearanceDefines::TEX_HAIR_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_EYES:
					result = LLAvatarAppearanceDefines::TEX_EYES_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LEFT_ARM:
					result = LLAvatarAppearanceDefines::TEX_LEFT_ARM_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LEFT_LEG:
					result = LLAvatarAppearanceDefines::TEX_LEFT_LEG_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_SKIRT:
					result = LLAvatarAppearanceDefines::TEX_SKIRT_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX1:
					result = LLAvatarAppearanceDefines::TEX_AUX1_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX2:
					result = LLAvatarAppearanceDefines::TEX_AUX2_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX3:
					result = LLAvatarAppearanceDefines::TEX_AUX3_TATTOO;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_UNDERPANTS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_UNDERPANTS;
			}

			break;
		}

		case LLWearableType::WT_UNDERSHIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_UNDERSHIRT;
			}

			break;
		}

		default:
		{
			llwarns << "Unknown wearable type: " << (S32)type
				    << " - Baked texture index: " << (S32)baked_texind
					<< " - Filename: " << mFilename
					<< " - TrackingID: " << mTrackingID
					<< " - InworldID: " << mWorldID << llendl;
		}
	}

	return result;
}

//static
void LLLocalBitmap::addUnitsCallback(HBFileSelector::ELoadFilter type,
									std::deque<std::string>& files, void*)
{
	bool updated = false;

	while (!files.empty())
	{
		std::string filename = files.front();
		files.pop_front();

		if (!filename.empty())
		{
			sTimer.stopTimer();

			LLLocalBitmap* bitmapp = new LLLocalBitmap(filename);
			if (bitmapp && bitmapp->getValid())
			{
				sBitmapList.push_back(bitmapp);
				updated = true;
			}
			else
			{
				LLSD notif_args;
				notif_args["FNAME"] = filename;
				gNotifications.add("LocalBitmapsVerifyFail", notif_args);
				delete bitmapp;
			}

			sTimer.startTimer();
		}
	}

	if (updated)
	{
		++sBitmapsListVersion;
	}
}

//static
void LLLocalBitmap::addUnits()
{
	HBFileSelector::loadFiles(HBFileSelector::FFLOAD_IMAGE, addUnitsCallback);
}

//static
void LLLocalBitmap::delUnit(const LLUUID& tracking_id)
{
	bool updated = false;

	for (list_t::iterator iter = sBitmapList.begin(), end = sBitmapList.end();
		 iter != end; )
	{
		list_t::iterator curiter = iter++;
		LLLocalBitmap* bitmapp = *curiter;
		if (bitmapp->getTrackingID() == tracking_id)
		{
			// std::list:erase() preserves all iterators but curiter
			sBitmapList.erase(curiter);
			delete bitmapp;
			updated = true;
		}
	}

	if (updated)
	{
		++sBitmapsListVersion;
	}
}

//static
const LLUUID& LLLocalBitmap::getWorldID(const LLUUID& tracking_id)
{
	for (list_t::const_iterator it = sBitmapList.begin(),
								end = sBitmapList.end();
		 it != end; ++it)
	{
		LLLocalBitmap* bitmapp = *it;
		if (bitmapp && bitmapp->getTrackingID() == tracking_id)
		{
			return bitmapp->getWorldID();
		}
	}
	return LLUUID::null;
}

//static
bool LLLocalBitmap::isLocal(const LLUUID& world_id)
{
	for (list_t::const_iterator it = sBitmapList.begin(),
								end = sBitmapList.end();
		 it != end; ++it)
	{
		LLLocalBitmap* bitmapp = *it;
		if (bitmapp && bitmapp->getWorldID() == world_id)
		{
			return true;
		}
	}
	return false;
}

//static
const std::string& LLLocalBitmap::getFilename(const LLUUID& tracking_id)
{
	for (list_t::const_iterator it = sBitmapList.begin(),
								end = sBitmapList.end();
		 it != end; ++it)
	{
		LLLocalBitmap* bitmapp = *it;
		if (bitmapp && bitmapp->getTrackingID() == tracking_id)
		{
			return bitmapp->getFilename();
		}
	}
	return LLStringUtil::null;
}

//static
void LLLocalBitmap::doUpdates()
{
	// Preventing theoretical overlap in cases of huge number of loaded images.
	sTimer.stopTimer();
	sNeedsRebake = false;

	for (list_t::iterator iter = sBitmapList.begin(), end = sBitmapList.end();
		 iter != end; ++iter)
	{
		(*iter)->updateSelf();
	}

	doRebake();
	sTimer.startTimer();
}

//static
void LLLocalBitmap::setNeedsRebake()
{
	sNeedsRebake = true;
}

// Separated that from doUpdates to insure a rebake can be called separately
// during deletion
//static
void LLLocalBitmap::doRebake()
{
	if (sNeedsRebake)
	{
		gAgentAvatarp->forceBakeAllTextures(true);
		sNeedsRebake = false;
	}
}

//static
boost::signals2::connection LLLocalBitmap::setOnChangedCallback(const LLUUID& id,
																const changed_cb_t& cb)
{
	for (list_t::iterator it = sBitmapList.begin(), end = sBitmapList.end();
		 it != end; ++it)
	{
		LLLocalBitmap* bitmapp = *it;
		if (bitmapp->mTrackingID == id)
		{
			return bitmapp->setChangedCallback(cb);
		}
	}
	return boost::signals2::connection();
}

//static
void LLLocalBitmap::associateGLTFMaterial(const LLUUID& id,
										  LLGLTFMaterial* matp)
{
	for (list_t::iterator it = sBitmapList.begin(), end = sBitmapList.end();
		 it != end; ++it)
	{
		LLLocalBitmap* bitmapp = *it;
		if (bitmapp->mTrackingID == id)
		{
			bitmapp->addGLTFMaterial(matp);
			// There should be only one such tracking Id in the list... HB
			return;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLLocalBitmapTimer class
///////////////////////////////////////////////////////////////////////////////

LLLocalBitmapTimer::LLLocalBitmapTimer()
:	LLEventTimer(LL_LOCAL_TIMER_HEARTBEAT)
{
}

void LLLocalBitmapTimer::startTimer()
{
	mEventTimer.start();
}

void LLLocalBitmapTimer::stopTimer()
{
	mEventTimer.stop();
}

bool LLLocalBitmapTimer::isRunning()
{
	return mEventTimer.getStarted();
}

bool LLLocalBitmapTimer::tick()
{
	LLLocalBitmap::doUpdates();
	return false;
}
