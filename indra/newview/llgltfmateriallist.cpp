/**
 * @file llgltfmateriallist.cpp
 * @brief The LLGLTFMaterialList class implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

#include "tinygltf/tiny_gltf.h"
// JsonCpp includes
#include "reader.h"
#include "value.h"

#include "llgltfmateriallist.h"

#include "indra_constants.h"				// For BLANK_MATERIAL_ASSET_ID
#include "llassetstorage.h"
#include "llcorehttputil.h"
#include "llfilesystem.h"
#include "llsdserialize.h"
#include "llworkqueue.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llpipeline.h"
#include "lltinygltfhelper.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvocache.h"
#include "llworld.h"

LLGLTFMaterialList gGLTFMaterialList;

LLGLTFMaterialList::modify_queue_t LLGLTFMaterialList::sModifyQueue;
LLGLTFMaterialList::apply_queue_t LLGLTFMaterialList::sApplyQueue;
LLSD LLGLTFMaterialList::sUpdates;
LLGLTFMaterialList::selection_cb_list_t LLGLTFMaterialList::sSelectionCallbacks;

///////////////////////////////////////////////////////////////////////////////
// LLGLTFMaterialList class
///////////////////////////////////////////////////////////////////////////////

void LLGLTFMaterialList::applyQueuedOverrides(LLViewerObject* objp)
{
	if (objp)
	{
		// The override cache is the authoritative source of the most recent
		// override data
		LLViewerRegion* regionp = objp->getRegion();
		if (regionp)
		{
			regionp->applyCacheMiscExtras(objp);
		}
	}
}

void LLGLTFMaterialList::queueModify(const LLViewerObject* objp, S32 side,
									 const LLGLTFMaterial* matp)
{
	if (!objp || objp->getRenderMaterialID(side).isNull())
	{
		return;
	}

	if (matp)
	{
		sModifyQueue.emplace_back(objp->getID(), *matp, side, true);
		return;
	}

	sModifyQueue.emplace_back(objp->getID(), LLGLTFMaterial(), side, false);
}

void LLGLTFMaterialList::queueApply(const LLViewerObject* objp, S32 side,
									const LLUUID& asset_id)
{
	const LLGLTFMaterial* matp = objp->getTE(side)->getGLTFMaterialOverride();
	if (matp)
	{
		LLGLTFMaterial* cleared_matp = new LLGLTFMaterial(*matp);
		cleared_matp->setBaseMaterial();
		sApplyQueue.emplace_back(objp->getID(), asset_id, cleared_matp, side);
		return;
	}
	sApplyQueue.emplace_back(objp->getID(), asset_id, nullptr, side);
}

void LLGLTFMaterialList::queueApply(const LLViewerObject* objp, S32 side,
									const LLUUID& asset_id,
									const LLGLTFMaterial* matp)
{
	if (!matp || asset_id.isNull())
	{
		queueApply(objp, side, asset_id);
		return;
	}
	sApplyQueue.emplace_back(objp->getID(), asset_id,
							 new LLGLTFMaterial(*matp), side);
}

void LLGLTFMaterialList::queueUpdate(const LLSD& data)
{
	if (!sUpdates.isArray())
	{
		sUpdates = LLSD::emptyArray();
	}
	sUpdates[sUpdates.size()] = data;
}

void LLGLTFMaterialList::flushUpdates(done_cb_t callback)
{
	LLSD& data = sUpdates;

	S32 i = data.size();

	for (ModifyMaterialData& e : sModifyQueue)
	{
		data[i]["object_id"] = e.object_id;
		data[i]["side"] = e.side;

		if (e.has_override)
		{
			data[i]["gltf_json"] = e.override_data.asJSON();
		}
		else
		{
			// Clear all overrides
			data[i]["gltf_json"] = "";
		}

		++i;
	}
	sModifyQueue.clear();

	for (auto& e : sApplyQueue)
	{
		data[i]["object_id"] = e.object_id;
		data[i]["side"] = e.side;
		data[i]["asset_id"] = e.asset_id;
		if (e.override_data)
		{
			data[i]["gltf_json"] = e.override_data->asJSON();
		}
		else
		{
			// Clear all overrides
			data[i]["gltf_json"] = "";
		}

		++i;
	}
	sApplyQueue.clear();

	if (sUpdates.size() == 0)
	{
		return;
	}

	const std::string& cap_url =
		gAgent.getRegionCapability("ModifyMaterialParams");
	if (cap_url.empty())
	{
		LL_DEBUGS("GLTF") << "No ModifyMaterialParams capability. Aborted"
						  << LL_ENDL;
		return;
	}

	gCoros.launch("modifyMaterialCoro",
				  boost::bind(&LLGLTFMaterialList::modifyMaterialCoro, cap_url,
							  sUpdates, callback));
	sUpdates = LLSD::emptyArray();
}

//static
void LLGLTFMaterialList::addSelectionUpdateCallback(update_cb_t callback)
{
	sSelectionCallbacks.push_back(callback);
}

//static
void LLGLTFMaterialList::doSelectionCallbacks(const LLUUID& obj_id, S32 side)
{
	for (auto& callback : sSelectionCallbacks)
	{
		callback(obj_id, side);
	}
}

struct GLTFAssetLoadUserData
{
	GLTFAssetLoadUserData() = default;

	LL_INLINE GLTFAssetLoadUserData(tinygltf::Model model,
									LLFetchedGLTFMaterial* matp)
	:	mModelIn(model),
		mMaterial(matp)
	{
	}

	tinygltf::Model						mModelIn;
	LLPointer<LLFetchedGLTFMaterial>	mMaterial;
};

// Work done via the general queue thread pool.
//static
bool LLGLTFMaterialList::decodeAsset(const LLUUID& id,
									 GLTFAssetLoadUserData* asset_data)
{
	LLFileSystem file(id);
	S32 size = file.getSize();
	if (!size)
	{
		llwarns << "Cannot read asset cache file for " << id << llendl;
		return false;
	}

	std::string buffer(size + 1, '\0');
	file.read((U8*)buffer.data(), size);

	// Read file into buffer
	std::stringstream llsdstream(buffer);
	LLSD asset;
	if (!LLSDSerialize::deserialize(asset, llsdstream, -1))
	{
		llwarns << "Failed to deserialize material LLSD for " << id << llendl;
		return false;
	}
	if (!asset.has("version"))
	{
		llwarns << "Missing GLTF version in material LLSD for " << id
				<< llendl;
		return false;
	}
	std::string data = asset["version"].asString();
	if (!LLGLTFMaterial::isAcceptedVersion(data))
	{
		llwarns << "Unsupported GLTF version " << data << " for " << id
				<< llendl;
		return false;
	}
	if (!asset.has("type"))
	{
		llwarns << "Missing GLTF asset type in material LLSD for " << id
				<< llendl;
		return false;
	}
	data = asset["type"].asString();
	if (data != LLGLTFMaterial::ASSET_TYPE)
	{
		llwarns << "Incorrect GLTF asset type '" << data << "' for " << id
				<< llendl;
		return false;
	}
	if (!asset.has("data") || !asset["data"].isString())
	{
		llwarns << "Invalid GLTF asset data for " << id << llendl;
		return false;
	}
	data = asset["data"].asString();
	std::string warn_msg, error_msg;
	tinygltf::TinyGLTF gltf;
	if (!gltf.LoadASCIIFromString(&asset_data->mModelIn, &error_msg, &warn_msg,
								  data.c_str(), data.length(), ""))
	{
		llwarns << "Failed to decode material asset " << id
				<< ". tinygltf reports: \n" << warn_msg << "\n"
				<< error_msg << llendl;
		return false;
	}
	return true;
}

// Work on the main thread via the main loop work queue.
//static
void LLGLTFMaterialList::decodeAssetCallback(const LLUUID& id,
											 GLTFAssetLoadUserData* asset_data,
											 bool result)
{
	if (asset_data->mMaterial.isNull())	// Paranoia ?  HB
	{
		LL_DEBUGS("GLTF") << "NULL material returned for " << id << LL_ENDL;
		return;
	}

	if (result)
	{
		asset_data->mMaterial->setFromModel(asset_data->mModelIn,
											0/*only one index*/);
	}
	else
	{
		LL_DEBUGS("GLTF") << "Failed to get material " << id << LL_ENDL;
	}
	asset_data->mMaterial->materialComplete();
	delete asset_data;
}

void LLGLTFMaterialList::onAssetLoadComplete(const LLUUID& id,
											 LLAssetType::EType asset_type,
											 void* user_data, S32 status,
											 LLExtStat ext_status)
{
	GLTFAssetLoadUserData* asset_data = (GLTFAssetLoadUserData*)user_data;
	if (status != LL_ERR_NOERR)
	{
		llwarns << "Error getting material asset data: "
				<< LLAssetStorage::getErrorString(status)
				<< " (" << status << ")" << llendl;
		asset_data->mMaterial->materialComplete();
		delete asset_data;
		return;
	}

	if (!gMainloopWorkp)
	{
		// We are likely shutting down... HB
		return;
	}

	static LLWorkQueue::weak_t general_queue =
		LLWorkQueue::getNamedInstance("General");

	gMainloopWorkp->postTo(general_queue,
						   // Work done on general queue
						   [id, asset_data]()
						   {
								return decodeAsset(id, asset_data);
						   },
						   // Callback to main thread
						   [id, asset_data](bool result)
						   {
								decodeAssetCallback(id, asset_data, result);
						   });
}

LLFetchedGLTFMaterial* LLGLTFMaterialList::getMaterial(const LLUUID& id)
{
	id_mat_map_t::iterator iter = mList.find(id);
	if (iter == mList.end())
	{
		LLFetchedGLTFMaterial* mat = new LLFetchedGLTFMaterial();
		mList[id] = mat;

		if (!mat->mFetching && gAssetStoragep)
		{
			mat->materialBegin();

			GLTFAssetLoadUserData* user_data = new GLTFAssetLoadUserData();
			user_data->mMaterial = mat;

			gAssetStoragep->getAssetData(id, LLAssetType::AT_MATERIAL,
										 onAssetLoadComplete, (void*)user_data);
		}

		return mat;
	}

	return iter->second;
}

void LLGLTFMaterialList::flushMaterials()
{
	// Similar variant to what textures use (TextureFetchUpdateMinCount in LL's
	// PBR viewer code).
	static LLCachedControl<U32> min_update_count(gSavedSettings,
												 "TextureFetchUpdateMinMediumPriority");
	// Update min_update_count or 5% of materials, whichever is greater
	U32 update_count = llmax((U32)min_update_count, mList.size() / 20);
	update_count = llmin(update_count, (U32)mList.size());

	constexpr F32 TIMEOUT = 30.f;
	F32 cur_time = gFrameTimeSeconds;

	// Advance iter one past the last key we updated
	id_mat_map_t::iterator iter = mList.find(mLastUpdateKey);
	if (iter != mList.end())
	{
		++iter;
	}

	while (update_count-- > 0)
	{
		if (iter == mList.end())
		{
			iter = mList.begin();
		}

		LLPointer<LLFetchedGLTFMaterial> material = iter->second;
		if (material->getNumRefs() == 2) // This one plus one from the list
		{

			if (!material->mActive && cur_time > material->mExpectedFlushTime)
			{
				iter = mList.erase(iter);
			}
			else
			{
				if (material->mActive)
				{
					material->mExpectedFlushTime = cur_time + TIMEOUT;
					material->mActive = false;
				}
				++iter;
			}
		}
		else
		{
			material->mActive = true;
			++iter;
		}
	}

	if (iter != mList.end())
	{
		mLastUpdateKey = iter->first;
	}
	else
	{
		mLastUpdateKey.setNull();
	}
}

//static
void LLGLTFMaterialList::modifyMaterialCoro(const std::string& cap_url,
											LLSD overrides, done_cb_t callback)
{
	LL_DEBUGS("GLTF") << "Applying override via ModifyMaterialParams cap: "
					  << overrides << LL_ENDL;

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("modifyMaterialCoro");
	LLSD result = adapter.postAndSuspend(cap_url, overrides, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);

	bool success = true;
	if (!status)
	{
		llwarns << "Failed to modify material." << llendl;
		success = false;
	}
	else if (!result["success"].asBoolean())
	{
		llwarns << "Failed to modify material: " << result["message"] << LL_ENDL;
		success = false;
	}

	if (callback)
	{
		callback(success);
	}
}

void LLGLTFMaterialList::applyOverrideMessage(LLMessageSystem* msg,
											  const std::string& data_in)
{
	if (!msg) return;	// Paranoia

	const LLHost& host = msg->getSender();
	LLViewerRegion* regionp = gWorld.getRegion(host);
	if (!regionp)
	{
		return;
	}

	std::stringstream llsdstream(data_in);
	LLSD data;
	LLSDSerialize::fromNotation(data, llsdstream, data_in.size());

	const LLSD& tes = data["te"];
	if (!tes.isArray())
	{
		llwarns_once << "Malformed message: no 'te' array." << llendl;
		return;
	}

	U32 local_id = data.get("id").asInteger();
	LLUUID id;
	gObjectList.getUUIDFromLocal(id, local_id, host.getAddress(),
								 host.getPort());
	LLViewerObject* objp = NULL;
	if (id.notNull())
	{
		LL_DEBUGS("GLTF") << "Received PBR material data for object: " << id
						  << LL_ENDL;
		objp = gObjectList.findObject(id);
		// Note: objp may be NULL  if the viewer has not heard about the object
		// yet...
		if (objp)
		{
			if (gShowObjectUpdates)
			{
				// Display a cyan blip for override updates when "Show objects
				// updates" is enabled.
				gPipeline.addDebugBlip(objp->getPositionAgent(),
									   LLColor4::cyan);
			}
		}
	}

	bool has_te[MAX_TES] = { false };

	LLGLTFOverrideCacheEntry entry;
	entry.mLocalId = local_id;
	entry.mRegionHandle = regionp->getHandle();

	const LLSD& od = data["od"];
	for (U32 i = 0, count = llmin(tes.size(), MAX_TES); i < count; ++i)
	{
		// Note: setTEGLTFMaterialOverride() and cache will take ownership.
		LLGLTFMaterial* matp = new LLGLTFMaterial();
		matp->applyOverrideLLSD(od[i]);
		S32 te = tes[i].asInteger();
		has_te[te] = true;
		entry.mSides.emplace(te, od[i]);
		entry.mGLTFMaterial.emplace(te, matp);
		if (objp)
		{
			objp->setTEGLTFMaterialOverride(te, matp);
			if (objp->getTE(te) && objp->getTE(te)->isSelected())
			{
				doSelectionCallbacks(id, te);
			}
		}
	}

	if (objp)
	{
		// Null out overrides on TEs that should not have them
		for (U32 i = 0, count = llmin(objp->getNumTEs(), MAX_TES); i < count;
			 ++i)
		{
			if (!has_te[i])
			{
				LLTextureEntry* tep = objp->getTE(i);
				if (tep && tep->getGLTFMaterialOverride())
				{
					objp->setTEGLTFMaterialOverride(i, NULL);
					doSelectionCallbacks(id, i);
				}
			}
		}
	}

	regionp->cacheFullUpdateGLTFOverride(entry);
}
