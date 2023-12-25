/**
 * @file llgltfmateriallist.h
 * @brief The LLGLTFMaterialList class declaration
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

#ifndef LL_LLGLTFMATERIALLIST_H
#define LL_LLGLTFMATERIALLIST_H

#include <list>

#include "llassettype.h"
#include "llextendedstatus.h"
#include "hbfastmap.h"

#include "llfetchedgltfmaterial.h"

class LLGLTFOverrideCacheEntry;
class LLMessageSystem;
class LLViewerObject;
struct GLTFAssetLoadUserData;

class LLGLTFMaterialList
{
	friend class LLGLTFMaterialOverrideDispatchHandler;

protected:
    LOG_CLASS(LLGLTFMaterialList);

public:
	typedef void (*done_cb_t)(bool success);
	typedef void (*update_cb_t)(const LLUUID& object_id, S32 side);

	LLGLTFMaterialList() = default;

	LLFetchedGLTFMaterial* getMaterial(const LLUUID& id);

	LL_INLINE void addMaterial(const LLUUID& id, LLFetchedGLTFMaterial* matp)
	{
		mList.emplace(id, matp);
	}

	LL_INLINE void removeMaterial(const LLUUID& id)
	{
		mList.erase(id);
	}

	void flushMaterials();

	// Queues a modification of a material that we want to send to the simulator.
	// Calls flushUpdates() to flush pending updates.
	//  id: ID of object to modify
	//  side: texure entry index to modify, or -1 for all sides
	//  mat: material to apply as override, or nullptr to remove existing
	//       overrides and revert to asset
	//
	// NOTE: do not use to revert to asset when applying a new asset id, use
	// queueApply below.
	static void queueModify(const LLViewerObject* objp, S32 side,
							const LLGLTFMaterial* matp);

	// Queues an application of a material asset we want to send to the simulator.
	// Calls flushUpdates() to flush pending updates.
	//  object_id: Id of object to apply material asset to
	//  side: texure entry index to apply material to, or -1 for all sides
	//  asset_id: Id of material asset to apply, or LLUUID::null to
	//            disassociate current material asset
	//
	// NOTE: Implicitly clears most override data if present
	static void queueApply(const LLViewerObject* objp, S32 side,
						   const LLUUID& asset_id);

	// Queues an application of a material asset we want to send to the simulator.
	// Calls flushUpdates() to flush pending updates.
	//  object_id: Id of object to apply material asset to
	//  side: texure entry index to apply material to, or -1 for all sides
	//  asset_id: Id of material asset to apply, or LLUUID::null to
	//            disassociate current material asset
	//  matp: override material, if NULL, will clear most override data.
	static void queueApply(const LLViewerObject* objp, S32 side,
						   const LLUUID& asset_id, const LLGLTFMaterial* matp);

	// Flushes pending material updates to the simulator. Automatically called
	// once per frame, but may be called explicitly for cases that care about
	// the done_callback forwarded to gCoros.launch()
	static void flushUpdates(done_cb_t callback = NULL);

	static void addSelectionUpdateCallback(update_cb_t callback);

	// Queues an explicit LLSD ModifyMaterialParams update apply given override
	// data overrides LLSD map (or array of maps) in the format:
	//  object_id: UUID(required)	 Id of object
	//  side:      integer(required) TE index of face to set, or -1 for all
	//  gltf_json: string(optional)	 override data to set, empty string nulls
	//                               out override data, omissions of this
	//                               parameter keeps existing data.
	//  asset_id:  UUID(optional)    Id of material asset to set: omission of
	//                               this parameter keeps existing material
	//                               asset Id
	//
	// NOTE: Unless you already have a gltf_json string you want to send,
	// strongly prefer using queueModify. If the queue/flush API is
	// insufficient, extend it.
	static void queueUpdate(const LLSD& data);

	void applyOverrideMessage(LLMessageSystem* msg, const std::string& data);

	// Called by batch builder to give LLGLTMaterialList an opportunity to
	// apply any override data that arrived before the object was ready to
	// receive it
	void applyQueuedOverrides(LLViewerObject* objp);

	static void doSelectionCallbacks(const LLUUID& object_id, S32 side);

private:
	static void modifyMaterialCoro(const std::string& url, LLSD overrides,
								   done_cb_t callback);

	// Called on onAssetLoadComplete() via the general queue thread pool.
	static bool decodeAsset(const LLUUID& id,
							GLTFAssetLoadUserData* asset_data);
	// Called on onAssetLoadComplete() on the main thread via the main loop
	// work queue.
	static void decodeAssetCallback(const LLUUID& id,
									GLTFAssetLoadUserData* asset_data,
									bool result);

protected:
	static void onAssetLoadComplete(const LLUUID& asset_uuid,
									LLAssetType::EType type,
									void* user_data,
									S32 status, LLExtStat ext_status);

protected:
	typedef fast_hmap<LLUUID, LLPointer<LLFetchedGLTFMaterial> > id_mat_map_t;
	id_mat_map_t				mList;

	LLUUID						mLastUpdateKey;

	struct ModifyMaterialData
	{
		LL_INLINE ModifyMaterialData()
		:	side(-1),
			has_override(false)
		{
		}

		LL_INLINE ModifyMaterialData(const LLUUID& id,
									 const LLGLTFMaterial& data,
									 S32 s, bool overridden)
		:	object_id(id),
			override_data(data),
			side(s),
			has_override(overridden)
		{
		}

		LLUUID				object_id;
		LLGLTFMaterial		override_data;
		S32					side;
		bool				has_override;
	};

	typedef std::list<ModifyMaterialData> modify_queue_t;
	static modify_queue_t	sModifyQueue;

	struct ApplyMaterialAssetData
	{
		LL_INLINE ApplyMaterialAssetData()
		:	side(-1)
		{
		}

		LL_INLINE ApplyMaterialAssetData(const LLUUID& oid,const LLUUID& aid,
										 LLGLTFMaterial* data, S32 s)
		:	object_id(oid),
			asset_id(aid),
			override_data(data),
			side(s)
		{
		}

		LLUUID						object_id;
		LLUUID						asset_id;
		LLPointer<LLGLTFMaterial>	override_data;
		S32							side;
	};

	typedef std::list<ApplyMaterialAssetData> apply_queue_t;
	static apply_queue_t		sApplyQueue;

	// Data to be flushed to ModifyMaterialParams capability
	static LLSD					sUpdates;

	typedef std::vector<update_cb_t> selection_cb_list_t;
	static selection_cb_list_t	sSelectionCallbacks;
};

extern LLGLTFMaterialList gGLTFMaterialList;

#endif	// LL_LLGLTFMATERIALLIST_H
