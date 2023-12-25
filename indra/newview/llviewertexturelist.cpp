/**
 * @file llviewertexturelist.cpp
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

#include "llviewerprecompiledheaders.h"

#include <sys/stat.h>
#include <utility>

#include "llviewertexturelist.h"

#include "imageids.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llimagedecodethread.h"
#include "llimagegl.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llimagetga.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llmessage.h"
#include "llsdserialize.h"
#include "llxmltree.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawpoolbump.h"				// For LLStandardBumpmap::init()
#include "llpipeline.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewertexture.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"				// For getShowProgress()

// Global variables

// Static members

F32 LLViewerTextureList::sLastTeleportTime = 0.f;
F32 LLViewerTextureList::sFetchingBoostFactor = 0.f;

void (*LLViewerTextureList::sUUIDCallback)(void**, const LLUUID&) = NULL;

U32 LLViewerTextureList::sTextureBits = 0;
U32 LLViewerTextureList::sTexturePackets = 0;
S32 LLViewerTextureList::sNumImages = 0;
S32 LLViewerTextureList::sUpdatedThisFrame = 0;
LLStat LLViewerTextureList::sNumImagesStat(32, true);
LLStat LLViewerTextureList::sNumUpdatesStat(15, true);
LLStat LLViewerTextureList::sNumRawImagesStat(32, true);
LLStat LLViewerTextureList::sGLTexMemStat(32, true);
LLStat LLViewerTextureList::sGLBoundMemStat(32, true);

LLViewerTextureList gTextureList;

LLViewerTexture* gImgPixieSmall = NULL;

LLViewerTextureList::LLViewerTextureList()
:	mForceResetTextureStats(false),
	mMaxResidentTexMemInMegaBytes(0),
	mMaxTotalTextureMemInMegaBytes(0),
	mLastGLImageCleaning(0.f),
	mInitialized(false),
	mFlushOldImages(false)
{
}

void LLViewerTextureList::init()
{
	mInitialized = true;
	sNumImages = 0;
	mMaxResidentTexMemInMegaBytes = 0;
	mMaxTotalTextureMemInMegaBytes = 0;

	// Update how much texture RAM we are allowed to use.
	updateMaxResidentTexMem(0); // 0 = use current

	llassert_always(mInitialized && mImageList.empty() && mUUIDMap.empty());

	llinfos << "Preloading images (any crash would be the result of a missing image file)..."
			<< llendl;

	// Set the "white" image
	LLViewerFetchedTexture* image =
		LLViewerTextureManager::getFetchedTextureFromFile("white.tga",
														  MIPMAP_NO,
														  LLGLTexture::BOOST_BUMP);
	llassert_always(image);
	image->dontDiscard();
#if !LL_IMPLICIT_SETNODELETE
	image->setNoDelete();
#endif
	LLViewerFetchedTexture::sWhiteImagep = image;

	// Set the default flat normal map
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("flatnormal.tga",
														  MIPMAP_NO,
														  LLGLTexture::BOOST_BUMP);
	llassert_always(image);
	image->dontDiscard();
#if !LL_IMPLICIT_SETNODELETE
	image->setNoDelete();
#endif
	LLViewerFetchedTexture::sFlatNormalImagep = image;

	// Set the default PBR irradiance map
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("default_irradiance.png",
														  MIPMAP_NO,
														  LLGLTexture::BOOST_BUMP);
	llassert_always(image);
	image->dontDiscard();
	LLViewerFetchedTexture::sDefaultIrradiancePBRp = image;

	LLUIImageList::getInstance()->initFromFile();

	image = LLViewerTextureManager::getFetchedTextureFromFile("silhouette.j2c");
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	mImagePreloads.emplace_back(image);

	image = LLViewerTextureManager::getFetchedTextureFromFile("noentrylines.j2c");
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	mImagePreloads.emplace_back(image);

	image = LLViewerTextureManager::getFetchedTextureFromFile("noentrypasslines.j2c");
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	mImagePreloads.emplace_back(image);

	// DEFAULT_WATER_OPAQUE
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("43c32285-d658-1793-c123-bf86315de055.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	LLViewerFetchedTexture::sOpaqueWaterImagep = image;

	// DEFAULT_WATER_TEXTURE
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("2bfd3884-7e27-69b9-ba3a-3e673f680004.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	LLViewerFetchedTexture::sWaterImagep = image;

	// DEFAULT_WATER_NORMAL
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("822ded49-9a6c-f61c-cb89-6df54f42cdf4.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	LLViewerFetchedTexture::sWaterNormapMapImagep = image;

	// Default Sun
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("cce0f112-878f-4586-a2e2-a8f104bba271.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_CLAMP);
	LLViewerFetchedTexture::sDefaultSunImagep = image;

	// Default Moon
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("d07f6eed-b96a-47cd-b51d-400ad4a1c428.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_CLAMP);
	LLViewerFetchedTexture::sDefaultMoonImagep = image;

	// Default clouds
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("fc4b9f0b-d008-45c6-96a4-01dd947ac621.tga",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_CLOUDS);
	llassert_always(image);
	image->dontDiscard();
#if !LL_IMPLICIT_SETNODELETE
	image->setNoDelete();
#endif
	LLViewerFetchedTexture::sDefaultCloudsImagep = image;

	// Default clouds noise
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("clouds2.tga",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_CLOUDS);
	llassert_always(image);
	image->dontDiscard();
#if !LL_IMPLICIT_SETNODELETE
	image->setNoDelete();
#endif
	LLViewerFetchedTexture::sDefaultCloudNoiseImagep = image;

	// Bloom
	image =
		LLViewerTextureManager::getFetchedTextureFromFile("3c59f7fe-9dc8-47f9-8aaf-a9dd1fbc3bef.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_CLAMP);
	LLViewerFetchedTexture::sBloomImagep = image;

	image =
		LLViewerTextureManager::getFetchedTextureFromFile("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI,
														  LLViewerTexture::FETCHED_TEXTURE,
														  0, 0,
														  IMG_TRANSPARENT);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_WRAP);
	mImagePreloads.emplace_back(image);

	image =
		LLViewerTextureManager::getFetchedTextureFromFile("alpha_gradient.tga",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI,
														  LLViewerTexture::FETCHED_TEXTURE,
														  GL_ALPHA8, GL_ALPHA,
														  IMG_ALPHA_GRAD);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_CLAMP);
	mImagePreloads.emplace_back(image);

	image =
		LLViewerTextureManager::getFetchedTextureFromFile("alpha_gradient_2d.j2c",
														  MIPMAP_YES,
														  LLGLTexture::BOOST_UI,
														  LLViewerTexture::FETCHED_TEXTURE,
														  GL_ALPHA8, GL_ALPHA,
														  IMG_ALPHA_GRAD_2D);
	llassert_always(image);
	image->setAddressMode(LLTexUnit::TAM_CLAMP);
	mImagePreloads.emplace_back(image);

	image = LLViewerTextureManager::getFetchedTextureFromFile("pixiesmall.j2c");
	llassert_always(image);
	mImagePreloads.emplace_back(image);
	gImgPixieSmall = image;

	LLUIImage::initClass();

	llinfos << "Images preloading successful." << llendl;
}

static std::string get_texture_list_name()
{
	static LLCachedControl<bool> last_loc(gSavedSettings, "LoginLastLocation");
	std::string filename = "texture_list_";
	filename += (last_loc ? "last" : "home");
	filename += ".xml";
	return gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, filename);
}

void LLViewerTextureList::doPrefetchImages()
{
	// Prefetch specific UUIDs, used by the sim server for some particle
	// systems defaults.
	LLViewerTextureManager::getFetchedTexture(IMG_SHOT);
	LLViewerTextureManager::getFetchedTexture(IMG_SMOKE_POOF);

	LLStandardBumpmap::init();

	if (gAppViewerp->getPurgeCache())
	{
		// Cache was purged, no point to pre-fetch last used cached images
		return;
	}

	// Pre-fetch textures from last logout
	LLSD imagelist;
	std::string filename = get_texture_list_name();
	llifstream file(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::fromXML(imagelist, file);
	}
	for (LLSD::array_iterator iter = imagelist.beginArray();
		 iter != imagelist.endArray(); ++iter)
	{
		LLSD imagesd = *iter;
		LLUUID uuid = imagesd["uuid"];
		S32 pixel_area = imagesd["area"];
		S32 texture_type = imagesd["type"];

		if (LLViewerTexture::FETCHED_TEXTURE == texture_type ||
			LLViewerTexture::LOD_TEXTURE == texture_type)
		{
			LLViewerFetchedTexture* image =
				LLViewerTextureManager::getFetchedTexture(uuid, FTT_DEFAULT,
														  MIPMAP_YES,
														  LLGLTexture::BOOST_NONE,
														  texture_type);
			if (image)
			{
				image->addTextureStats((F32)pixel_area);
			}
		}
	}
}

void LLViewerTextureList::shutdown()
{
	// Clear out preloads
	gImgPixieSmall = NULL;
	mImagePreloads.clear();

	// Write out list of currently loaded textures for precaching on startup
	typedef std::set<std::pair<S32, LLViewerFetchedTexture*> > image_area_list_t;
	image_area_list_t image_area_list;
	for (priority_list_t::iterator iter = mImageList.begin();
		 iter != mImageList.end(); ++iter)
	{
		LLViewerFetchedTexture* image = *iter;
		if (image->getID() == IMG_DEFAULT || image->getFTType() != FTT_DEFAULT)
		{
			continue;
		}
		S8 type = image->getType();
		if (type != LLViewerTexture::FETCHED_TEXTURE &&
			type != LLViewerTexture::LOD_TEXTURE)
		{
			continue;
		}
		if (!image->hasGLTexture() || !image->getUseDiscard() ||
			image->needsAux() || !image->getBoundRecently())
		{
			continue;
		}
		S32 desired = image->getDesiredDiscardLevel();
		if (desired >= 0 && desired < MAX_DISCARD_LEVEL)
		{
			S32 pixel_area = image->getWidth(desired) *
							 image->getHeight(desired);
			image_area_list.emplace(pixel_area, image);
		}
	}

	LLSD imagelist;
	constexpr S32 max_count = 1000;
	S32 count = 0;
	for (image_area_list_t::reverse_iterator riter = image_area_list.rbegin();
		 riter != image_area_list.rend(); ++riter)
	{
		LLViewerFetchedTexture* image = riter->second;
		LLSD& entry = imagelist[count];
		entry["area"] = riter->first;
		entry["uuid"] = image->getID();
		entry["type"] = S32(image->getType());
		if (++count >= max_count)
		{
			break;
		}
	}

	if (count > 0 && !gDirUtilp->getLindenUserDir().empty())
	{
		std::string filename = get_texture_list_name();
		llofstream file(filename.c_str());
		if (file.is_open())
		{
			LLSDSerialize::toPrettyXML(imagelist, file);
			file.close();
		}
		else
		{
			llwarns << "Could not open file '" << filename << "' for writing."
					<< llendl;
		}
	}

	// Clean up "loaded" callbacks.
	mCallbackList.clear();

	// Flush all of the references
	mCreateTextureList.clear();

	mUUIDMap.clear();

	mImageList.clear();

	LLUIImage::cleanupClass();

	// Prevent loading textures again.
	mInitialized = false;
}

void LLViewerTextureList::dump()
{
	llinfos << "Image list begin dump:" << llendl;
	for (priority_list_t::iterator it = mImageList.begin(),
								   end = mImageList.end();
		 it != end; ++it)
	{
		LLViewerFetchedTexture* image = *it;
		if (!image) continue;
		llinfos << "priority " << image->getDecodePriority()
				<< " boost " << image->getBoostLevel()
				<< " size " << image->getWidth() << "x" << image->getHeight()
				<< " discard " << image->getDiscardLevel()
				<< " desired " << image->getDesiredDiscardLevel()
				<< " http://asset.siva.lindenlab.com/" << image->getID()
				<< ".texture" << llendl;
	}
	llinfos << "Image list end dump" << llendl;
}

//static
void LLViewerTextureList::destroyGL(bool save_state)
{
	LLImageGL::destroyGL(save_state);
}

//static
void LLViewerTextureList::restoreGL()
{
	llassert_always(gTextureList.mInitialized);
	LLImageGL::restoreGL();
}

LLViewerFetchedTexture* LLViewerTextureList::getImageFromFile(const std::string& filename,
															  bool usemipmaps,
															  LLGLTexture::EBoostLevel boost_priority,
															  S8 texture_type,
															  S32 internal_format,
															  U32 primary_format,
															  const LLUUID& force_id)
{
	if (!mInitialized)
	{
		return NULL;
	}

	std::string full_path = gDirUtilp->findSkinnedFilename("textures",
														   filename);
	if (full_path.empty())
	{
		llwarns << "Failed to find local image file: " << filename << llendl;
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	std::string url = "file://" + full_path;

	return getImageFromUrl(url, FTT_LOCAL_FILE, usemipmaps, boost_priority,
						   texture_type, internal_format, primary_format,
						   force_id);
}

LLViewerFetchedTexture* LLViewerTextureList::getImageFromUrl(const std::string& url,
															 FTType f_type,
															 bool usemipmaps,
															 LLGLTexture::EBoostLevel boost_priority,
															 S8 texture_type,
															 S32 internal_format,
															 U32 primary_format,
															 const LLUUID& force_id)
{

	if (!mInitialized)
	{
		return NULL;
	}

	// Generate UUID based on hash of filename
	LLUUID new_id;
	if (force_id.notNull())
	{
		new_id = force_id;
	}
	else
	{
		new_id.generate(url);
	}

	LLPointer<LLViewerFetchedTexture> imagep = findImage(new_id);
	if (imagep.isNull())
	{
		switch (texture_type)
		{
			case LLViewerTexture::FETCHED_TEXTURE:
				imagep = new LLViewerFetchedTexture(url, f_type, new_id,
													usemipmaps);
				break;

			case LLViewerTexture::LOD_TEXTURE:
				imagep = new LLViewerLODTexture(url, f_type, new_id,
												usemipmaps);
				break;

			default:
				llerrs << "Invalid texture type " << texture_type << llendl;
		}

		if (internal_format && primary_format)
		{
			imagep->setExplicitFormat(internal_format, primary_format);
		}

		addImage(imagep);

		if (boost_priority != 0)
		{
			imagep->setBoostLevel(boost_priority);
		}
	}
	else
	{
		LLViewerFetchedTexture* texp = imagep.get();
		if (!texp)	// Paranoia
		{
			llwarns << "Image " << new_id
					<< " does not have a texture pointer !" << llendl;
		}
		else if (texp->getUrl().empty())
		{
			std::string type;
			switch (texture_type)
			{
				case LLViewerTexture::FETCHED_TEXTURE:
					type = "FETCHED_TEXTURE";
					break;

				case LLViewerTexture::LOD_TEXTURE:
					type = "LOD_TEXTURE";
					break;

				default:
					type = "unknown";
			}
			llwarns << "Requested texture " << new_id << " of type " << type
					<< " already exists but does not have an URL." << llendl;
			if (!url.empty())
			{
				llinfos << "Setting new URL and forcing a refetch of "
						<< new_id << llendl;
				texp->setUrl(url);
				texp->forceRefetch();
			}
		}
		else if (texp->getUrl() != url)
		{
			// This is not an error as long as the images really match -
			// e.g. could be two avatars wearing the same outfit.
			LL_DEBUGS("ViewerTexture") << "Requested texture " << new_id
									   << " already exists with a different url, requested: "
									   << url << " - current: "
									   << texp->getUrl() << LL_ENDL;
		}
	}

	imagep->setGLTextureCreated(true);

	return imagep;
}


// Returns the image with ID image_id. If the image is not found, creates a new
// image and enqueues a request for transmission
LLViewerFetchedTexture* LLViewerTextureList::getImage(const LLUUID& image_id,
													  FTType f_type,
													  bool usemipmaps,
													  LLGLTexture::EBoostLevel boost_priority,
													  S8 texture_type,
													  S32 internal_format,
													  U32 primary_format,
													  LLHost from_host)
{
	if (!mInitialized)
	{
		return NULL;
	}

	if (image_id.isNull())
	{
		return LLViewerFetchedTexture::sDefaultImagep;
	}

	LLPointer<LLViewerFetchedTexture> imagep = findImage(image_id);
	if (imagep.isNull())
	{
		imagep = createImage(image_id, f_type, usemipmaps, boost_priority,
							 texture_type, internal_format, primary_format,
							 from_host);
	}
	else
	{
		if (boost_priority != LLGLTexture::BOOST_ALM &&
			imagep->getBoostLevel() == LLGLTexture::BOOST_ALM)
		{
			// We need BOOST_ALM texture for something, 'rise' to NONE
			imagep->setBoostLevel(LLGLTexture::BOOST_NONE);
		}
		if (from_host.isOk())
		{
			LLViewerFetchedTexture* texp = imagep.get();
			if (!texp->getTargetHost().isOk())
			{
				// Common and normal occurrence with default textures such as
				// IMG_INVISIBLE. Made into a debug message to prevent useless
				// log spam.
				LL_DEBUGS("ViewerTexture") << "Requested texture " << image_id
										   << " already exists but does not have a host."
										   << LL_ENDL;
			}
			else if (from_host != texp->getTargetHost())
			{
				llwarns << "Requested texture " << image_id
						<< " already exists with a different target host, requested: "
						<< from_host << " - current: "
						<< texp->getTargetHost() << llendl;
			}
		}
	}

	imagep->setGLTextureCreated(true);

	return imagep;
}

// When this function is called, there is no such texture in the gTextureList
// with image_id.
LLViewerFetchedTexture* LLViewerTextureList::createImage(const LLUUID& image_id,
														 FTType f_type,
														 bool usemipmaps,
														 LLGLTexture::EBoostLevel boost_priority,
														 S8 texture_type,
														 S32 internal_format,
														 U32 primary_format,
														 LLHost from_host)
{
	LLPointer<LLViewerFetchedTexture> texp;
	switch (texture_type)
	{
		case LLViewerTexture::FETCHED_TEXTURE:
			texp = new LLViewerFetchedTexture(image_id, f_type, from_host,
											  usemipmaps);
			break;

		case LLViewerTexture::LOD_TEXTURE:
			texp = new LLViewerLODTexture(image_id, f_type, from_host,
										  usemipmaps);
			break;

		default:
			llerrs << "Invalid texture type " << texture_type << llendl;
	}

	if (internal_format && primary_format)
	{
		texp->setExplicitFormat(internal_format, primary_format);
	}

	addImage(texp);

	if (boost_priority)
	{
		texp->setBoostLevel(boost_priority);
	}
	else
	{
		// By default, the texture can not be removed from memory even if it is
		// not used. Here turn this off. If this texture should be set to
		// NO_DELETE, call setNoDelete() afterwards.
		texp->forceActive();
	}

	return texp;
}

LLViewerFetchedTexture* LLViewerTextureList::findImage(const LLUUID& image_id)
{
	uuid_map_t::iterator iter = mUUIDMap.find(image_id);
	if (iter != mUUIDMap.end())
	{
		return iter->second;
	}
	return NULL;
}

void LLViewerTextureList::addImageToList(LLViewerFetchedTexture* image)
{
	llassert(image);
	if (image->isInImageList())
	{
		llwarns << "Image already in list" << llendl;
		llassert(false);
	}
	if ((mImageList.insert(image)).second != true)
	{
		llwarns << "An error occurred while inserting image into mImageList"
				<< llendl;
		llassert(false);
	}

	image->setInImageList(true);
}

void LLViewerTextureList::removeImageFromList(LLViewerFetchedTexture* image)
{
	llassert(image);

	S32 count = 0;
	if (image->isInImageList())
	{
		count = mImageList.erase(image);
		if (count != 1)
		{
			llwarns << "Image  " << image->getID()
					<< " had mInImageList set but mImageList.erase() returned "
					<< count << llendl;
		}
	}
	else
	{
		// Something is wrong, image is expected in list or callers should
		// check first
		llwarns << "Called for " << image->getID()
				<< " but does not have mInImageList set. Ref count is "
				<< image->getNumRefs() << llendl;

		uuid_map_t::iterator iter = mUUIDMap.find(image->getID());
		if (iter == mUUIDMap.end())
		{
			llwarns << "Image " << image->getID() << " is not in mUUIDMap !"
					<< llendl;
		}
		else if (iter->second != image)
		{
			llwarns << "Image  " << image->getID()
					<< " was in mUUIDMap but with different pointer" << llendl;
		}
		else
		{
			llwarns << "Image  " << image->getID()
					<< " was in mUUIDMap with same pointer" << llendl;
		}
		count = mImageList.erase(image);
		if (count)
		{
			llwarns << "Image " << image->getID()
					<< " had mInImageList false but mImageList.erase() returned "
					<< count << llendl;
		}
		llassert(false);
	}

	image->setInImageList(false);
}

void LLViewerTextureList::addImage(LLViewerFetchedTexture* new_image)
{
	if (!new_image)
	{
		return;
	}
	LLUUID image_id = new_image->getID();

	LLViewerFetchedTexture* image = findImage(image_id);
	if (image)
	{
		llwarns << "Image with ID " << image_id << " already in list"
				<< llendl;
	}
	++sNumImages;

	addImageToList(new_image);
	mUUIDMap.emplace(image_id, new_image);
}

void LLViewerTextureList::deleteImage(LLViewerFetchedTexture* image)
{
	if (image)
	{
		if (image->hasCallbacks())
		{
			mCallbackList.erase(image);
		}
		if (mUUIDMap.erase(image->getID()) != 1)
		{
			llwarns << "Deleted texture " << image->getID()
					<< " was not in the UUIDs list !" << llendl;
			llassert(false);
		}
		--sNumImages;
		removeImageFromList(image);
	}
}

void LLViewerTextureList::dirtyImage(LLViewerFetchedTexture* image)
{
	mDirtyTextureList.insert(image);
}

//static
void LLViewerTextureList::resetFrameStats()
{
	sNumUpdatesStat.addValue(sUpdatedThisFrame);
	sUpdatedThisFrame = 0;
}

void LLViewerTextureList::updateImages(F32 max_time)
{
	if (gTeleportDisplay)
	{
		// Do not update images during teleports
		return;
	}

	bool can_queue = LLImageGLThread::sEnabled && gMainloopWorkp;

	if (can_queue)
	{
		// Subtract 1ms used to update the GL worker thread
		max_time -= 0.001f;
	}

	gTextureFetchp->setTextureBandwidth(gViewerStats.mTextureKBitStat.getMeanPerSec());

	sNumImagesStat.addValue(sNumImages);
	sNumRawImagesStat.addValue(LLImageRaw::sRawImageCount);
	sGLTexMemStat.addValue((F32)BYTES2MEGABYTES(LLImageGL::sGlobalTexMemBytes));
	sGLBoundMemStat.addValue((F32)BYTES2MEGABYTES(LLImageGL::sBoundTexMemBytes));

	updateImagesDecodePriorities();

	static LLCachedControl<F32> allowed(gSavedSettings,
										"TextureUpdateMinAllowedTime");
	// Split in two equal minimum time slots clamped between 1ms and 50ms each.
	F32 min_time = llclamp((F32)allowed * 0.5f, 0.001f, 0.05f);

	// Update texture fetches.
	max_time = llmax(max_time, min_time);
	max_time -= updateImagesFetchTextures(max_time);

	// Update the new textures that must be created in the main thread.
	max_time = llmax(max_time, min_time);
	updateImagesCreateTextures(max_time);

	if (!mDirtyTextureList.empty())
	{
		LL_FAST_TIMER(FTM_IMAGE_MARK_DIRTY);
		gPipeline.dirtyPoolObjectTextures(mDirtyTextureList);
		mDirtyTextureList.clear();
	}

	{
		LL_FAST_TIMER(FTM_IMAGE_CALLBACKS);
		bool didone = false;
		for (callback_list_t::iterator iter = mCallbackList.begin(),
									   end = mCallbackList.end();
			 iter != end; )
		{
			// Trigger loaded callbacks on local textures immediately
			LLViewerFetchedTexture* image = *iter++;
			if (!image->getUrl().empty())
			{
				// Do stuff to handle callbacks, update priorities, etc.
				didone = image->doLoadedCallbacks();
			}
			else if (!didone)
			{
				// Do stuff to handle callbacks, update priorities, etc.
				didone = image->doLoadedCallbacks();
			}
		}
	}

	updateImagesUpdateStats();

	// Update the main work queue if needed. HB
	if (can_queue && !gMainloopWorkp->empty())
	{
		static auto one_ms = std::chrono::milliseconds(1);
		size_t remaining = 0;
		gMainloopWorkp->runFor(one_ms, &remaining);
		LLViewerFetchedTexture::sImageThreadQueueSize = remaining;
	}
	else
	{
		LLViewerFetchedTexture::sImageThreadQueueSize = 0;
	}
}

void LLViewerTextureList::clearFetchingRequests()
{
	if (!gTextureFetchp || !gTextureFetchp->getNumRequests())
	{
		return;
	}

	uuid_list_t deleted_ids = gTextureFetchp->deleteAllRequests();

	for (priority_list_t::iterator iter = mImageList.begin(),
								   end = mImageList.end();
		 iter != end; ++iter)
	{
		LLViewerFetchedTexture* image = *iter;
		if (image && deleted_ids.count(image->getID()))
		{
			image->requestWasDeleted();
		}
	}
}

// Updates the decode priority for N images each frame
void LLViewerTextureList::updateImagesDecodePriorities()
{
	LL_FAST_TIMER(FTM_IMAGE_UPDATE_PRIO);

	// This algorithm ensures that old (unused) NO_DELETE textures are
	// re-ACTIVATEd, so to free up the VRAM. HB
	static LLCachedControl<U32> min_clean_delay(gSavedSettings,
												"StaleGLImageCleanupMinDelay");
	static LLCachedControl<U32> max_clean_delay(gSavedSettings,
												"StaleGLImageCleanupMaxDelay");
	if (min_clean_delay)	// Disabled when set to 0
	{
		F32 min_cleaning_delay = llmin((F32)min_clean_delay, 30.f);
		F32 max_cleaning_delay = 999999.f;	// No max delay by default.
		if (max_clean_delay)
		{
			max_cleaning_delay = llmax((F32)max_clean_delay,
									   min_cleaning_delay);
		}
		// Still clean up every now and then, when we have a high pressure on
		// texture memory.
		else if (LLViewerTexture::sDesiredDiscardBias >= 3.f)
		{
			max_cleaning_delay = llmin(10.f * min_cleaning_delay, 60.f);
		}
		F32 last_cleaning_age = gFrameTimeSeconds - mLastGLImageCleaning;
		if ((mFlushOldImages || last_cleaning_age > max_cleaning_delay) &&
			(last_cleaning_age > min_cleaning_delay ||
			 LLViewerTexture::sDesiredDiscardBias >= 4.5f))
		{
			mLastGLImageCleaning = gFrameTimeSeconds;
			U32 activated = LLImageGL::activateStaleTextures();
			if (activated)
			{
				LL_DEBUGS("TextureCleanup") << "Reactivated " << activated
											<< " staled NO_DELETE textures."
											<< LL_ENDL;
			}
		}
	}

	static LLCachedControl<U32> boost_after_tp(gSavedSettings,
											   "TextureFetchBoostTimeAfterTP");
	static LLCachedControl<bool> boost_with_speed(gSavedSettings,
												  "TextureFetchBoostWithSpeed");
	static LLCachedControl<bool> boost_with_fetches(gSavedSettings,
													"TextureFetchBoostWithFetches");
	static LLCachedControl<F32> high_prio_factor(gSavedSettings,
												 "TextureFetchBoostHighPrioFactor");
	static LLCachedControl<U32> fetch_ratio(gSavedSettings,
											"TextureFetchBoostRatioPerFetch");
	static LLCachedControl<U32> updates_per_sec(gSavedSettings,
												"TextureFetchUpdatePrioPerSec");
	static LLCachedControl<U32> max_high_prio(gSavedSettings,
											  "TextureFetchUpdateHighPriority");
	static LLCachedControl<U32> max_updates(gSavedSettings,
											"TextureFetchUpdateMaxMediumPriority");
	static LLCachedControl<U32> min_updates(gSavedSettings,
											"TextureFetchUpdateMinMediumPriority");
	static LLCachedControl<F32> upd_ratio(gSavedSettings,
										  "TextureUpdateBoostRatioPerDiscard");
	// When showing the progress view, reset image last seen timer to avoid
	// removing prefetched textures too soon.
	bool reset_timer = gViewerWindowp && gViewerWindowp->getShowProgress();

	// Note: do not boost textures decoding in low memory conditions, to avoid
	// a yo-yo effect with discard bias and the ensuing constant redecoding. HB
	F32 factor = 1.f;
	if (!mFlushOldImages && !LLViewerTexture::inLowMemCondition())
	{
		if (gFrameTimeSeconds - sLastTeleportTime < (F32)boost_after_tp)
		{
			factor = 4.f;
		}
		else
		{
			if (boost_with_speed)
			{
				F32 cam_moving_speed = gViewerCamera.getAverageSpeed();
				F32 cam_angular_speed = gViewerCamera.getAverageAngularSpeed();
				factor = llmax(0.25f * cam_moving_speed,
							   2.f * cam_angular_speed - 1) + 1.f;
				factor = llmin(factor, 4.f);
			}
			U32 num_fetches = gTextureFetchp->getApproxNumRequests();
			if (boost_with_fetches && fetch_ratio)
			{
				factor = llclamp(F32(num_fetches) / F32(fetch_ratio),
								 factor, 4.f);
			}
		}
	}
	sFetchingBoostFactor = factor;

	F32 update_priority_per_sec = (F32)updates_per_sec * factor;
	mUpdateHighPriority = (F32)max_high_prio * factor *
						  llclamp((F32)high_prio_factor, 1.f, 4.f);
	mUpdateMaxMediumPriority = (F32)max_updates * factor;
	mUpdateMinMediumPriority = (F32)min_updates * factor;

	// Target between update_priority_per_sec and upd_ratio/2+1 times that
	// number of textures per second, depending on discard bias (the highest
	// the bias, the more textures we check so to delete unused ones faster).
	// HB
	F32 max_update_count = llclamp(update_priority_per_sec, 256.f, 4096.f);
	F32 uratio = llclamp((F32)upd_ratio, 0.f, 1.f) * 0.5f;
	max_update_count = (max_update_count * gFrameIntervalSeconds + 1.f) *
					   (1.f + LLViewerTexture::sDesiredDiscardBias * uratio);
	S32 map_size = mUUIDMap.size();
	S32 update_counter = llmin((S32)max_update_count, map_size);
	sUpdatedThisFrame += update_counter;

	// Compute the max inactive time, based on the discard bias level (the
	// higher that level, the sooner unused textures are flushed so to free
	// memory faster).
	static LLCachedControl<U32> timeout(gSavedSettings,
										"TextureLazyFlushTimeout");
	F32 max_inactive_time =
		llmax(10.f,
			  (F32)timeout /
			  (1.f + LLViewerTexture::sDesiredDiscardBias * 0.5f));
	uuid_map_t::iterator iter = mUUIDMap.upper_bound(mLastUpdateUUID);
	while ((update_counter-- > 0 || (mFlushOldImages && map_size-- > 0)) &&
		   !mUUIDMap.empty())
	{
		if (iter == mUUIDMap.end())
		{
			iter = mUUIDMap.begin();
		}
		LLPointer<LLViewerFetchedTexture> imagep = iter->second;
		if (imagep.isNull())
		{
			llwarns << "NULL texture pointer found in list for texture Id: "
					<< iter->first << ". Removing." << llendl;
			iter = mUUIDMap.erase(iter);
			--map_size;
			continue;
		}
		mLastUpdateUUID = iter++->first;

		// Flush formatted images using a lazy flush

		F32 last_referenced = 0.f;
		if (!reset_timer)
		{
			last_referenced = imagep->getElapsedLastReferenceTime();
		}

		// 1 for mImageList, 1 for mUUIDMap, 1 for local reference:
		constexpr S32 MIN_REFS = 3;
		S32 num_refs = imagep->getNumRefs();
		if (num_refs <= MIN_REFS)
		{
			if (last_referenced > max_inactive_time * 0.5f)
			{
				if (imagep->hasFetcher())
				{
					LL_DEBUGS("TextureCleanup") << "Removing stale fetcher for texture: "
												<< imagep->getID()
												<< LL_ENDL;
					gTextureFetchp->deleteRequest(imagep->getID());
				}
				// Remove the unused image from the image list
				deleteImage(imagep);
				imagep = NULL; // Should destroy the image
			}
			continue;
		}

		if (imagep->hasSavedRawImage() &&
			imagep->getElapsedLastReferencedSavedRawImageTime() >
				max_inactive_time)
		{
			imagep->destroySavedRawImage();
		}

		if (imagep->isDeleted())
		{
			continue;
		}
		if (imagep->isDeletionCandidate())
		{
			if (imagep->destroyTexture())
			{
				continue;
			}
			// Cannot delete this image since it is actually waiting for GL
			// image creation. Setting it INACTIVE (setActive() now, then
			// setInactive() below). HB
			LL_DEBUGS("TextureCleanup") << "Texture " << imagep->getID()
										<< " could not be deleted. Setting INACTIVE."
										<< LL_ENDL;
			imagep->setActive();
		}
		else if (!reset_timer && imagep->isInactive())
		{
			if (last_referenced > max_inactive_time)
			{
				imagep->setDeletionCandidate();
			}
			continue;
		}
		// Images are often indirectly set NO_DELETE and this prevents them
		// from being removed from memory while they are not in use any more.
		// Let's make them active again to allow removal when actually needed.
		// HB
		else if (last_referenced > max_inactive_time && imagep->isNoDelete())
		{
			U32 boost_level = imagep->getBoostLevel();
			// Never touch textures used by the UI, map, media, bumps, etc...
			if (boost_level < LLGLTexture::BOOST_UI &&
				// ... neither sculpties.
				boost_level != LLGLTexture::BOOST_SCULPTED)
			{
				LL_DEBUGS("TextureCleanup") << "Setting old NO_DELETE texture "
											<< imagep->getID() << " ACTIVE."
											<< LL_ENDL;
				imagep->forceActive();
			}
		}

		imagep->resetLastReferencedTime();
		// Set texture state to INACTIVE, if currently ACTIVE.
		imagep->setInactive();

		if (!imagep->isInImageList())
		{
			continue;
		}

		if (update_counter >= 0 && !mFlushOldImages)
		{
			imagep->processTextureStats();
			F32 old_priority = imagep->getDecodePriority();
			F32 old_priority_test = llmax(old_priority, 0.f);
			F32 decode_priority = imagep->calcDecodePriority();
			F32 decode_priority_test = llmax(decode_priority, 0.f);
			// Ignore < 20% difference
			if (decode_priority_test < old_priority_test * .8f ||
				decode_priority_test > old_priority_test * 1.25f)
			{
				mImageList.erase(imagep);
				imagep->setDecodePriority(decode_priority);
				// Do not use imagep after this call !  HB
				mImageList.emplace(std::move(imagep));
			}
		}
	}

	mFlushOldImages = false;
}

// Created GL textures for all textures that need them (images which have been
// decoded, but have not been pushed into GL).
F32 LLViewerTextureList::updateImagesCreateTextures(F32 max_time)
{
	if (gGLManager.mIsDisabled)
	{
		return 0.f;
	}

	LL_FAST_TIMER(FTM_IMAGE_CREATE);

	LLTimer create_timer;

	image_list_t::iterator enditer = mCreateTextureList.begin();
	for (image_list_t::iterator iter = mCreateTextureList.begin(),
								end = mCreateTextureList.end();
		 iter != end; )
	{
		image_list_t::iterator curiter = iter++;
		enditer = iter;
		LLViewerFetchedTexture* imagep = *curiter;
		imagep->createTexture();
		imagep->postCreateTexture();
		if (create_timer.getElapsedTimeF32() > max_time)
		{
			break;
		}
	}
	mCreateTextureList.erase(mCreateTextureList.begin(), enditer);

	return create_timer.getElapsedTimeF32();
}

void LLViewerTextureList::forceImmediateUpdate(LLViewerFetchedTexture* imagep)
{
	if (!imagep)
	{
		return;
	}
	if (imagep->isInImageList())
	{
		removeImageFromList(imagep);
	}

	imagep->processTextureStats();
	F32 decode_priority = LLViewerFetchedTexture::maxDecodePriority();
	imagep->setDecodePriority(decode_priority);
	addImageToList(imagep);
}

F32 LLViewerTextureList::updateImagesFetchTextures(F32 max_time)
{
	LL_FAST_TIMER(FTM_IMAGE_FETCH);

	LLTimer image_op_timer;

	// Update fetch for N images each frame
	static LLCachedControl<F32> threshold(gSavedSettings,
										  "TextureFetchUpdatePriorityThreshold");
	bool skip_low_prio = (F32)threshold > 0.f;

	S32 max_priority_count = llmin((S32)(mUpdateHighPriority *
										 mUpdateHighPriority *
										 gFrameIntervalSeconds) + 1,
								   (S32)mUpdateHighPriority);
	max_priority_count = llmin(max_priority_count, (S32)mImageList.size());

	S32 total_update_count = mUUIDMap.size();
	S32 max_update_count = llmin((S32)(mUpdateMaxMediumPriority *
									   mUpdateMaxMediumPriority *
									   gFrameIntervalSeconds) + 1,
								 (S32)mUpdateMaxMediumPriority);
	max_update_count = llmin(max_update_count, total_update_count);

	// max_high_prio high priority entries
	typedef std::vector<LLViewerFetchedTexture*> entries_list_t;
	static entries_list_t entries;
	entries.clear();
	entries.reserve(max_priority_count);
	S32 update_counter = max_priority_count;
	priority_list_t::iterator iter1 = mImageList.begin();
	while (update_counter-- > 0)
	{
		entries.push_back(*iter1++);
	}

	// max_update_count cycled entries
	static U32 skipped = 0;
	update_counter = max_update_count;
	if (update_counter > 0)
	{
		uuid_map_t::iterator iter2 = mUUIDMap.upper_bound(mLastFetchUUID);
		while (update_counter > 0 && total_update_count-- > 0)
		{
			if (iter2 == mUUIDMap.end())
			{
				iter2 = mUUIDMap.begin();
			}
			LLViewerFetchedTexture* imagep = iter2->second;
			++iter2;
			// Skip the textures where there is really nothing to do so to give
			// some times to others. Also skip the texture if it is already in
			// the high prio set.
			if (skip_low_prio && imagep->getDecodePriority() <= threshold &&
				!imagep->hasFetcher())
			{
				++skipped;
			}
            else
            {
                entries.push_back(imagep);
                --update_counter;
            }
		}
	}

	S32 min_update_count = llmin((S32)mUpdateMinMediumPriority,
								 (S32)(entries.size() - max_priority_count));
	S32 min_count = max_priority_count + min_update_count;
	LLViewerFetchedTexture* imagep = NULL;
	for (U32 i = 0, count = entries.size(); i < count; ++i)
	{
		imagep = entries[i];
		imagep->updateFetch();
		if (min_count <= 0 && image_op_timer.getElapsedTimeF32() > max_time)
		{
			break;
		}
		--min_count;
	}
	if (imagep && min_count <= min_update_count)
	{
		mLastFetchUUID = imagep->getID();
	}

	// Report the number of skipped low priority texture updates, but do so in
	// a non-spammy way (once a second, when the corresponding debug flag is
	// set).
	static F32 last_report = 0.f;
	if (skipped && gFrameTimeSeconds - last_report > 1.f)
	{
		LL_DEBUGS("ViewerTexture") << "Skipped " << skipped
								   << " low priority textures update fetches."
								   << LL_ENDL;
		skipped = 0;
		last_report = gFrameTimeSeconds;
	}

	return image_op_timer.getElapsedTimeF32();
}

void LLViewerTextureList::updateImagesUpdateStats()
{
	LL_FAST_TIMER(FTM_IMAGE_STATS);

	if (mForceResetTextureStats)
	{
		for (priority_list_t::iterator iter = mImageList.begin(),
									   end = mImageList.end();
			 iter != end; )
		{
			LLViewerFetchedTexture* imagep = *iter++;
			imagep->resetTextureStats();
		}
		mForceResetTextureStats = false;
	}
}

S32 LLViewerTextureList::decodeAllImages(F32 max_time)
{
	LLTimer timer;

	// Update texture stats and priorities
	std::vector<LLPointer<LLViewerFetchedTexture> > image_list;
	for (priority_list_t::iterator iter = mImageList.begin(),
								   end = mImageList.end();
		 iter != end; )
	{
		LLViewerFetchedTexture* imagep = *iter++;
		image_list.push_back(imagep);
		imagep->setInImageList(false);
	}
	mImageList.clear();
	for (std::vector<LLPointer<LLViewerFetchedTexture> >::iterator
			iter = image_list.begin(), end = image_list.end();
		 iter != end; ++iter)
	{
		LLViewerFetchedTexture* imagep = *iter;
		imagep->processTextureStats();
		F32 decode_priority = imagep->calcDecodePriority();
		imagep->setDecodePriority(decode_priority);
		addImageToList(imagep);
	}
	image_list.clear();

	// Update fetch (decode)
	for (priority_list_t::iterator iter = mImageList.begin(),
								   end = mImageList.end();
		 iter != end; )
	{
		LLViewerFetchedTexture* imagep = *iter++;
		imagep->updateFetch();
	}

	// Run threads
	bool can_queue = LLImageGLThread::sEnabled && gMainloopWorkp;
	S32 fetch_pending = 0;
	do
	{
		// Un-pauses the texture cache thread
		gTextureCachep->update();
		// Un-pauses the texture fetch thread
		fetch_pending = gTextureFetchp->update();
		// Service the threaded work queues
		if (can_queue)
		{
			static auto one_ms = std::chrono::milliseconds(1);
			size_t remaining = 0;
			gMainloopWorkp->runFor(one_ms, &remaining);
			fetch_pending += remaining;
			LLViewerFetchedTexture::sImageThreadQueueSize = remaining;
		}
	}
	while (fetch_pending && timer.getElapsedTimeF32() < max_time);

	// Update fetch again
	for (priority_list_t::iterator iter = mImageList.begin(),
								   end = mImageList.end();
		 iter != end; )
	{
		LLViewerFetchedTexture* imagep = *iter++;
		imagep->updateFetch();
	}
	max_time -= timer.getElapsedTimeF32();
	max_time = llmax(max_time, 0.1f);
	F32 create_time = updateImagesCreateTextures(max_time);

	LL_DEBUGS("ViewerTexture") << "decodeAllImages() took "
							   << timer.getElapsedTimeF32()
							   << " seconds - fetch_pending = "
							   << fetch_pending << " - create_time = "
							   << create_time << LL_ENDL;

	return fetch_pending;
}

bool LLViewerTextureList::createUploadFile(const std::string& filename,
										   const std::string& out_filename,
										   U8 codec)
{
	// First, load the image.
	LLPointer<LLImageRaw> raw_image = new LLImageRaw;

	switch (codec)
	{
		case IMG_CODEC_BMP:
		{
			LLPointer<LLImageBMP> bmp_image = new LLImageBMP;

			if (!bmp_image->load(filename))
			{
				return false;
			}

			if (!bmp_image->decode(raw_image))
			{
				return false;
			}
			break;
		}

		case IMG_CODEC_TGA:
		{
			LLPointer<LLImageTGA> tga_image = new LLImageTGA;

			if (!tga_image->load(filename))
			{
				return false;
			}

			if (!tga_image->decode(raw_image))
			{
				return false;
			}

			if (tga_image->getComponents() != 3 &&
				tga_image->getComponents() != 4)
			{
				tga_image->setLastError("Image files with less than 3 or more than 4 components are not supported.");
				return false;
			}
			break;
		}

		case IMG_CODEC_JPEG:
		{
			LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;

			if (!jpeg_image->load(filename))
			{
				return false;
			}

			if (!jpeg_image->decode(raw_image))
			{
				return false;
			}
			break;
		}

		case IMG_CODEC_PNG:
		{
			LLPointer<LLImagePNG> png_image = new LLImagePNG;

			if (!png_image->load(filename))
			{
				return false;
			}

			if (!png_image->decode(raw_image))
			{
				return false;
			}
			break;
		}

		default:
			return false;
	}

	LLPointer<LLImageJ2C> j2cp = convertToUploadFile(raw_image);
	if (j2cp.isNull() || !j2cp->save(out_filename))
	{
		llinfos << "Could not create output file " << out_filename << llendl;
		return false;
	}

	// Test to see if the encode and save worked.
	LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
	if (!integrity_test->loadAndValidate(out_filename))
	{
		llinfos << "Image: " << out_filename << " is corrupt." << llendl;
		return false;
	}

	return true;
}

// WARNING: this method modifies the rawp image !
LLPointer<LLImageJ2C> LLViewerTextureList::convertToUploadFile(LLPointer<LLImageRaw> rawp,
															   S32 max_dimentions,
															   bool force_lossless)
{
	if (max_dimentions < 0)
	{
		max_dimentions = LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT;
	}
	rawp->biasedScaleToPowerOfTwo(max_dimentions);

	LLPointer<LLImageJ2C> j2cp = new LLImageJ2C();
	j2cp->setRate(0.f);

	constexpr S32 MAX_AREA = LL_IMAGE_REZ_LOSSLESS_CUTOFF *
							 LL_IMAGE_REZ_LOSSLESS_CUTOFF;
	static LLCachedControl<bool> lossless(gSavedSettings, "LosslessJ2CUpload");
	if (force_lossless ||
		(lossless && rawp->getWidth() * rawp->getHeight() <= MAX_AREA))
	{
		j2cp->setReversible(true);
	}

	if (!j2cp->encode(rawp))
	{
		llwarns << "Failure to encode as a J2C image !" << llendl;
		j2cp = NULL;
	}

	return j2cp;
}

// Returns min setting for TextureMemory (in MB)
//static
S32 LLViewerTextureList::getMinVideoRamSetting()
{
	// System memory in MB (used to be clamped to 4096MB for 32 bits builds via
	// a now removed getPhysicalMemoryKBClamped() call). HB
	S32 system_ram = LLMemory::getPhysicalMemoryKB() >> 10;
	// min texture mem sets to 64MB if total physical memory is more than
	// 1.5GB, and 32MB otherwise.
	return system_ram > 1500 ? 64 : 32;
}

// Returns max setting for TextureMemory (in MB)
//static
S32 LLViewerTextureList::getMaxVideoRamSetting(bool get_recommended)
{
	S32 max_texmem;
	U32 vram_manual_override = gSavedSettings.getU32("VRAMOverride");
	// In %, for integer divisions later. HB
	S32 multiplier = llclamp(100.f * gSavedSettings.getF32("TexMemMultiplier"),
							 100.f, 200.f);
	if (vram_manual_override > 32)
	{
		static U32 last_override = 0;
		if (vram_manual_override != last_override)
		{
			last_override = vram_manual_override;
			llwarns << "Overriding the detected VRAM amount with the VRAMOverride debug settings: "
					<< vram_manual_override << "MB of VRAM assumed." << llendl;
		}
		max_texmem = (multiplier * vram_manual_override) / 100;
	}
	else if (gGLManager.mTexVRAM)
	{
		max_texmem = gGLManager.mTexVRAM;
		// Treat any card with < 32 MB (shudder) as having 32 MB; it is going
		// to be swapping constantly regardless
		max_texmem = llmax(max_texmem, getMinVideoRamSetting());
		if (!get_recommended)
		{
			max_texmem = (multiplier * max_texmem) / 100;
		}
	}
	else if (gGLManager.mVRAM)
	{
		// 75% of total VRAM at most for textures.
		max_texmem = 3 * gGLManager.mVRAM / 4;
		// Treat any card with < 32 MB (shudder) as having 32 MB; it is going
		// to be swapping constantly regardless
		max_texmem = llmax(max_texmem, getMinVideoRamSetting());
		if (!get_recommended)
		{
			max_texmem = (multiplier * max_texmem) / 100;
		}
	}
	else
	{
		if (!get_recommended || gSavedSettings.getBool("NoHardwareProbe"))
		{
			max_texmem = 512;
		}
		else
		{
			max_texmem = 128;
		}
		llwarns << "VRAM amount not detected, defaulting to " << max_texmem
				<< " MB" << llendl;
	}

	// System memory in MB (used to be clamped to 4096MB for 32 bits builds via
	// a now removed getPhysicalMemoryKBClamped() call). HB
	S32 system_ram = LLMemory::getPhysicalMemoryKB() >> 10;
	if (get_recommended)
	{
		max_texmem = llmin(max_texmem, system_ram / 2);
#if 0	// *TODO, maybe: further clamp down to 4096MB ?
		max_texmem = llmin(max_texmem, 4096);
#endif
		llinfos << "Recommended max texture RAM: " << max_texmem
				<< " MB - System RAM: " << system_ram << " MB."<< llendl;
	}
	else
	{
		max_texmem = llmin(max_texmem, system_ram);
		llinfos << "Usable texture RAM: " << max_texmem
				<< " MB - System RAM: " << system_ram << " MB."<< llendl;
	}

	return max_texmem;
}

constexpr S32 VIDEO_CARD_FRAMEBUFFER_MEM = 12; // MB
constexpr S32 MIN_MEM_FOR_NON_TEXTURE = 512; //MB
void LLViewerTextureList::updateMaxResidentTexMem(S32 mem)
{
	// Initialize the image pipeline VRAM settings
	S32 cur_mem = gSavedSettings.getS32("TextureMemory");
	S32 default_mem = getMaxVideoRamSetting(true); // Recommended default
	if (mem == 0)
	{
		mem = cur_mem > 0 ? cur_mem : default_mem;
	}
	else if (mem < 0)
	{
		mem = default_mem;
	}

	// Limit the texture memory to a multiple of the default if we have found
	// some cards to behave poorly otherwise
	mem = llmin(mem, default_mem);

	// When asking for default, keep things reasonnable on modern gaphics cards
	// with more VRAM than what the viewer will ever need or be able to cope
	// with (see the MaxBoundTexMem limiting below). HB
	if (cur_mem <= 0 && mem > 3072)
	{
		mem = 3072;
	}

	S32 max_vram = getMaxVideoRamSetting();
	mem = llclamp(mem, getMinVideoRamSetting(), max_vram);
	if (mem != cur_mem)
	{
		gSavedSettings.setS32("TextureMemory", mem);
		// At this point the setting listener re-entered this method already.
		return;
	}

	// *TODO: set available resident texture mem based on use by other
	// subsystems currently max(12MB, llmin(VRAM/4, 512)) assumed...
	S32 vb_mem = mem;
	S32 fb_mem = llclamp(vb_mem / 4, VIDEO_CARD_FRAMEBUFFER_MEM, 512);
	mMaxResidentTexMemInMegaBytes = vb_mem - fb_mem; // in MB

	// Limit the total amount of textures to 1.25 * max_vram
	mMaxTotalTextureMemInMegaBytes = llmin(2 * mMaxResidentTexMemInMegaBytes,
										   (S32)(5 * max_vram / 4));

	S32 max_bound = llclamp((S32)gSavedSettings.getU32("MaxBoundTexMem"),
							512, 4096);
	if (mMaxResidentTexMemInMegaBytes > max_bound)
	{
		// Limit the amount of resident (GL bound) textures to something sane:
		// not doing so causes HUGE and NASTY slow downs in some conditions,
		// such as when rotating the camera in texture-heavy environments. HB
		mMaxResidentTexMemInMegaBytes = max_bound;
	}

	// System memory in MB (used to be clamped to 4096MB for 32 bits builds via
	// a now removed getPhysicalMemoryKBClamped() call). HB
	S32 system_ram = LLMemory::getPhysicalMemoryKB() >> 10;

	// Minimum memory reserved for non-texture use. If system_raw >= 1GB then
	// reserve at least 512MB for non-texture use, otherwise reserve half of
	// the system_ram for non-texture use.
	S32 min_non_texture_mem = llmin(system_ram / 2, MIN_MEM_FOR_NON_TEXTURE);

	if (mMaxTotalTextureMemInMegaBytes > system_ram - min_non_texture_mem)
	{
		mMaxTotalTextureMemInMegaBytes = system_ram - min_non_texture_mem;
	}

	llinfos << "Total usable VRAM: " << vb_mem << " MB"
			<< " - Usable frame buffers VRAM: " << fb_mem << " MB"
			<< " - Usable texture VRAM: " << vb_mem - fb_mem << " MB"
			<< " - Maximum total texture memory set to: "
			<< mMaxTotalTextureMemInMegaBytes << " MB"
			<< " - Maximum total GL bound texture memory set to: "
			<< mMaxResidentTexMemInMegaBytes << " MB"
			<< llendl;
}

// Receive image header, copy into image object and decompresses if this is a
// one-packet image.
//static
void LLViewerTextureList::receiveImageHeader(LLMessageSystem* msg, void**)
{
	LL_FAST_TIMER(FTM_PROCESS_IMAGES);

	char ip_string[256];
	u32_to_ip_string(msg->getSenderIP(), ip_string);

	U32 received_size;
	if (msg->getReceiveCompressedSize())
	{
		received_size = msg->getReceiveCompressedSize();
	}
	else
	{
		received_size = msg->getReceiveSize();
	}
	gTextureList.sTextureBits += received_size * 8;
	++gTextureList.sTexturePackets;

	LLUUID id;
	msg->getUUIDFast(_PREHASH_ImageID, _PREHASH_ID, id);
	U8 codec;
	msg->getU8Fast(_PREHASH_ImageID, _PREHASH_Codec, codec);
	U16 packets;
	msg->getU16Fast(_PREHASH_ImageID, _PREHASH_Packets, packets);
	U32 totalbytes;
	msg->getU32Fast(_PREHASH_ImageID, _PREHASH_Size, totalbytes);

	S32 data_size = msg->getSizeFast(_PREHASH_ImageData, _PREHASH_Data);
	if (data_size > 0)
	{
		// This buffer gets saved off in the packet list
		U8* data = new U8[data_size];
		msg->getBinaryDataFast(_PREHASH_ImageData, _PREHASH_Data, data,
							   data_size);

		LLViewerFetchedTexture* image =
			LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
													  LLGLTexture::BOOST_NONE,
													  LLViewerTexture::LOD_TEXTURE);
		if (!image ||
			!gTextureFetchp->receiveImageHeader(msg->getSender(), id, codec,
												packets, totalbytes, data_size,
												data))
		{
			delete[] data;
		}
	}
	else if (data_size < 0)
	{
		llwarns << "Invalid image header chunk size: " << data_size << llendl;
	}
}

// Receives image packet, copy into image object, checks if all packets
// received, decompresses if so.
//static
void LLViewerTextureList::receiveImagePacket(LLMessageSystem* msg, void**)
{
	LL_FAST_TIMER(FTM_PROCESS_IMAGES);

	char ip_string[256];
	u32_to_ip_string(msg->getSenderIP(), ip_string);

	U32 received_size;
	if (msg->getReceiveCompressedSize())
	{
		received_size = msg->getReceiveCompressedSize();
	}
	else
	{
		received_size = msg->getReceiveSize();
	}
	gTextureList.sTextureBits += received_size * 8;
	++gTextureList.sTexturePackets;

	LLUUID id;
	msg->getUUIDFast(_PREHASH_ImageID, _PREHASH_ID, id);
	U16 packet_num;
	msg->getU16Fast(_PREHASH_ImageID, _PREHASH_Packet, packet_num);

	S32 data_size = msg->getSizeFast(_PREHASH_ImageData, _PREHASH_Data);
	if (data_size > 0)
	{
		if (data_size > MTUBYTES)
		{
			llerrs << "Image data chunk too large: " << data_size << " bytes"
				   << llendl;
		}

		U8* data = new U8[data_size];
		msg->getBinaryDataFast(_PREHASH_ImageData, _PREHASH_Data, data,
							   data_size);

		LLViewerFetchedTexture* image =
			LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
												  	  LLGLTexture::BOOST_NONE,
												  	  LLViewerTexture::LOD_TEXTURE);
		if (!image ||
			!gTextureFetchp->receiveImagePacket(msg->getSender(), id,
												packet_num, data_size, data))
		{
			delete[] data;
		}
	}
	else if (data_size < 0)
	{
		llwarns << "Invalid image data chunk size: " << data_size << llendl;
	}
}

// We have been that the asset server does not contain the requested image id.
//static
void LLViewerTextureList::processImageNotInDatabase(LLMessageSystem* msg,
													void**)
{
	LL_FAST_TIMER(FTM_PROCESS_IMAGES);
	LLUUID image_id;

	msg->getUUIDFast(_PREHASH_ImageID, _PREHASH_ID, image_id);

	LLViewerFetchedTexture* image = gTextureList.findImage(image_id);
	if (image)
	{
		image->setIsMissingAsset();
	}
}

// Explicitly cleanup resources, as this is a singleton class with process
// lifetime so ability to perform std::map operations in destructor is not
// guaranteed.
void LLUIImageList::cleanUp()
{
	mUIImages.clear();
	mUITextureList.clear();
}

LLUIImagePtr LLUIImageList::getUIImageByID(const LLUUID& image_id)
{
	// Look for existing image, using the UUID as an image name
	uuid_ui_image_map_t::iterator it = mUIImages.find(image_id.asString());
	if (it != mUIImages.end())
	{
		LL_DEBUGS("GetUIImageCalls") << "Requested UI image UUID: " << image_id
									 << LL_ENDL;
		return it->second;
	}
	return loadUIImageByID(image_id);
}

LLUIImagePtr LLUIImageList::getUIImage(const std::string& name)
{
	// Look for existing image
	uuid_ui_image_map_t::iterator it = mUIImages.find(name);
	if (it != mUIImages.end())
	{
		LL_DEBUGS("GetUIImageCalls") << "Requested UI image: " << name
									 << LL_ENDL;
		return it->second;
	}

	return loadUIImageByName(name, name);
}

LLUIImagePtr LLUIImageList::loadUIImageByName(const std::string& name,
											  const std::string& filename,
											  bool use_mips,
											  const LLRect& scale_rect)
{
	LL_DEBUGS("GetUIImageCalls") << "Loaded UI image: " << name << LL_ENDL;
	LLViewerFetchedTexture* imagep =
		LLViewerTextureManager::getFetchedTextureFromFile(filename, MIPMAP_NO,
														  LLGLTexture::BOOST_UI);
	return loadUIImage(imagep, name, use_mips, scale_rect);
}

LLUIImagePtr LLUIImageList::loadUIImageByID(const LLUUID& id, bool use_mips,
											const LLRect& scale_rect)
{
	LL_DEBUGS("GetUIImageCalls") << "Loaded UI image UUID: " << id << LL_ENDL;

	LLViewerFetchedTexture* imagep =
		LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, MIPMAP_NO,
												  LLGLTexture::BOOST_UI);
	return loadUIImage(imagep, id.asString(), use_mips, scale_rect);
}

LLUIImagePtr LLUIImageList::loadUIImage(LLViewerFetchedTexture* imagep,
										const std::string& name, bool use_mips,
										const LLRect& scale_rect)
{
	if (!imagep) return NULL;

	imagep->setBoostLevel(LLGLTexture::BOOST_UI);
	imagep->setAddressMode(LLTexUnit::TAM_CLAMP);
	// Do not compress UI images
	imagep->getGLImage()->setAllowCompression(false);

	LLUIImagePtr new_imagep = new LLUIImage(name, imagep);
	mUIImages.emplace(name, new_imagep);
	mUITextureList.emplace_back(imagep);

#if 0	// LLGLTexture::ICON is not used in the Cool VL Viewer... HB
	// Note: some other textures such as ICON also go through this flow to be
	// fetched. But only UI textures need to set this callback.
	if (imagep->getBoostLevel() == LLGLTexture::BOOST_UI)
#endif
	{
		LLUIImageLoadData* datap = new LLUIImageLoadData;
		datap->mImageName = name;
		datap->mImageScaleRegion = scale_rect;

		imagep->setLoadedCallback(onUIImageLoaded, 0, false, false, datap,
								  NULL);
	}
	return new_imagep;
}

LLUIImagePtr LLUIImageList::preloadUIImage(const std::string& name,
										   const std::string& filename,
										   bool use_mips,
										   const LLRect& scale_rect)
{
	// Look for existing image
	uuid_ui_image_map_t::iterator found_it = mUIImages.find(name);
	if (found_it != mUIImages.end())
	{
		// Image already loaded !
		llerrs << "UI Image " << name << " already loaded." << llendl;
	}

	return loadUIImageByName(name, filename, use_mips, scale_rect);
}

//static
void LLUIImageList::onUIImageLoaded(bool success, LLViewerFetchedTexture* texp,
									LLImageRaw*, LLImageRaw*,
									S32 discard_level, bool is_final,
									void* datap)
{
	if (!success || !datap)
	{
		return;
	}

	LLUIImageLoadData* image_datap = (LLUIImageLoadData*)datap;
	std::string ui_image_name = image_datap->mImageName;
	LLRect scale_rect = image_datap->mImageScaleRegion;
	if (is_final)
	{
		delete image_datap;
	}
	if (!texp || texp->getUrl().compare(0, 7, "file://") != 0)
	{
		return;
	}

	LLUIImageList* self = getInstance();
	uuid_ui_image_map_t::iterator it = self->mUIImages.find(ui_image_name);
	if (it == self->mUIImages.end())
	{
		return;
	}

	LLUIImagePtr imagep = it->second;
	if (imagep.isNull())
	{
		return;
	}

	// For images grabbed from local files, apply clipping rectangle to restore
	// original dimensions from power-of-2 gl image
	F32 clip_x = (F32)texp->getOriginalWidth() / (F32)texp->getFullWidth();
	F32 clip_y = (F32)texp->getOriginalHeight() / (F32)texp->getFullHeight();
	imagep->setClipRegion(LLRectf(0.f, clip_y, clip_x, 0.f));

	if (scale_rect == LLRect::null)
	{
		return;
	}

	F32 width_div = 1.f / (F32)imagep->getWidth();
	F32 height_div = 1.f / (F32)imagep->getHeight();
	imagep->setScaleRegion(LLRectf(llclamp((F32)scale_rect.mLeft * width_div,
										   0.f, 1.f),
								   llclamp((F32)scale_rect.mTop * height_div,
										   0.f, 1.f),
								   llclamp((F32)scale_rect.mRight * width_div,
										   0.f, 1.f),
								   llclamp((F32)scale_rect.mBottom *
										   height_div,
										   0.f, 1.f)));
}

bool LLUIImageList::initFromFile()
{
	// Construct path to canonical textures.xml in default skin dir
	std::string base_file_path =
		gDirUtilp->getExpandedFilename(LL_PATH_SKINS, "default", "textures",
									   "textures.xml");
	LLXMLNodePtr root;

	if (!LLXMLNode::parseFile(base_file_path, root, NULL))
	{
		llwarns << "Unable to parse UI image list file " << base_file_path
				<< llendl;
		return false;
	}

	if (!root->hasAttribute("version"))
	{
		llwarns << "No valid version number in UI image list file "
				<< base_file_path << llendl;
		return false;
	}

	std::vector<std::string> paths;
	// Path to current selected skin
	paths.emplace_back(gDirUtilp->getSkinDir() + LL_DIR_DELIM_STR +
					   "textures" + LL_DIR_DELIM_STR + "textures.xml");
	// Path to user overrides on current skin
	paths.emplace_back(gDirUtilp->getUserSkinDir() + LL_DIR_DELIM_STR +
					   "textures" + LL_DIR_DELIM_STR + "textures.xml");

	// Apply skinned xml files incrementally
	for (std::vector<std::string>::iterator path_it = paths.begin();
		 path_it != paths.end(); ++path_it)
	{
		// Do not reapply base file to itself
		if (!path_it->empty() && *path_it != base_file_path)
		{
			LLXMLNodePtr update_root;
			if (LLXMLNode::parseFile(*path_it, update_root, NULL))
			{
				LLXMLNode::updateNode(root, update_root);
			}
		}
	}

	enum
	{
		PASS_DECODE_NOW,
		PASS_DECODE_LATER,
		NUM_PASSES
	};

	std::string file_name;
	for (S32 pass = PASS_DECODE_NOW; pass < NUM_PASSES; ++pass)
	{
		LLXMLNodePtr child_nodep = root->getFirstChild();
		while (child_nodep.notNull())
		{
			std::string image_name;
			child_nodep->getAttributeString("name", image_name);
			file_name = image_name;	// Use as default file name

			// Load high priority textures on first pass (to kick off decode)
			bool preload = false;
			child_nodep->getAttributeBool("preload", preload);
			if (preload)
			{
				if (pass == PASS_DECODE_LATER)
				{
					child_nodep = child_nodep->getNextSibling();
					continue;
				}
			}
			else if (pass == PASS_DECODE_NOW)
			{
				child_nodep = child_nodep->getNextSibling();
				continue;
			}

			child_nodep->getAttributeString("file_name", file_name);

			bool use_mip_maps = false;
			child_nodep->getAttributeBool("use_mips", use_mip_maps);

			LLRect scale_rect;
			child_nodep->getAttributeS32("scale_left", scale_rect.mLeft);
			child_nodep->getAttributeS32("scale_right", scale_rect.mRight);
			child_nodep->getAttributeS32("scale_bottom", scale_rect.mBottom);
			child_nodep->getAttributeS32("scale_top", scale_rect.mTop);

			preloadUIImage(image_name, file_name, use_mip_maps, scale_rect);

			child_nodep = child_nodep->getNextSibling();
		}

		if (pass == PASS_DECODE_NOW && !gSavedSettings.getBool("NoPreload"))
		{
			gTextureList.decodeAllImages(10.f); // Decode preloaded images
		}
	}
	return true;
}
